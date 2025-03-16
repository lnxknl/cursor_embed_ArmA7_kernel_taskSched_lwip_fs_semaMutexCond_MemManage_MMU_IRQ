#include "ipc.h"
#include "memory.h"
#include "task.h"
#include <string.h>

// 全局消息队列链表
static msg_queue_t *msg_queues = NULL;
static mutex_t msg_queues_lock;
static ipc_stats_t ipc_stats;

// 初始化消息队列系统
void msgq_init(void) {
    mutex_init(&msg_queues_lock, "msg_queues_lock");
    memset(&ipc_stats, 0, sizeof(ipc_stats));
}

// 查找消息队列
static msg_queue_t *find_msg_queue(key_t key) {
    msg_queue_t *mq = msg_queues;
    while (mq) {
        if (mq->key == key) {
            return mq;
        }
        mq = mq->next;
    }
    return NULL;
}

// 创建消息队列
int msgq_create(key_t key, uint32_t max_msgs, uint32_t max_size) {
    mutex_lock(&msg_queues_lock);

    // 检查是否已存在
    if (find_msg_queue(key)) {
        mutex_unlock(&msg_queues_lock);
        return -1;
    }

    // 分配消息队列结构
    msg_queue_t *mq = malloc(sizeof(msg_queue_t));
    if (!mq) {
        mutex_unlock(&msg_queues_lock);
        return -1;
    }

    // 分配消息缓冲区
    mq->buffer = malloc(max_msgs * max_size);
    if (!mq->buffer) {
        free(mq);
        mutex_unlock(&msg_queues_lock);
        return -1;
    }

    // 初始化消息队列
    mq->key = key;
    mq->max_msgs = max_msgs;
    mq->max_size = max_size;
    mq->msg_count = 0;
    mq->head = 0;
    mq->tail = 0;
    mutex_init(&mq->lock, "msgq_lock");
    condition_init(&mq->not_full, "msgq_not_full");
    condition_init(&mq->not_empty, "msgq_not_empty");

    // 添加到链表
    mq->next = msg_queues;
    msg_queues = mq;
    ipc_stats.msg_queues++;

    mutex_unlock(&msg_queues_lock);
    return 0;
}

// 发送消息
int msgq_send(int mqid, const msg_t *msg, uint32_t size, uint32_t timeout) {
    msg_queue_t *mq = find_msg_queue(mqid);
    if (!mq) return -1;

    mutex_lock(&mq->lock);

    // 等待直到队列非满
    while (mq->msg_count >= mq->max_msgs) {
        if (timeout == 0) {
            mutex_unlock(&mq->lock);
            return -1;
        }
        if (!condition_timedwait(&mq->not_full, &mq->lock, timeout)) {
            mutex_unlock(&mq->lock);
            return -1;
        }
    }

    // 复制消息到缓冲区
    uint32_t offset = mq->tail * mq->max_size;
    memcpy(mq->buffer + offset, msg, size);
    
    // 更新队列状态
    mq->tail = (mq->tail + 1) % mq->max_msgs;
    mq->msg_count++;
    ipc_stats.msg_sends++;

    // 通知等待的接收者
    condition_signal(&mq->not_empty);

    mutex_unlock(&mq->lock);
    return 0;
}

// 接收消息
int msgq_receive(int mqid, msg_t *msg, uint32_t size, long type, uint32_t timeout) {
    msg_queue_t *mq = find_msg_queue(mqid);
    if (!mq) return -1;

    mutex_lock(&mq->lock);

    // 等待直到队列非空
    while (mq->msg_count == 0) {
        if (timeout == 0) {
            mutex_unlock(&mq->lock);
            return -1;
        }
        if (!condition_timedwait(&mq->not_empty, &mq->lock, timeout)) {
            mutex_unlock(&mq->lock);
            return -1;
        }
    }

    // 查找指定类型的消息
    uint32_t current = mq->head;
    uint32_t count = mq->msg_count;
    bool found = false;

    while (count > 0) {
        msg_t *current_msg = (msg_t *)(mq->buffer + current * mq->max_size);
        if (type == 0 || current_msg->type == type) {
            found = true;
            break;
        }
        current = (current + 1) % mq->max_msgs;
        count--;
    }

    if (!found) {
        mutex_unlock(&mq->lock);
        return -1;
    }

    // 复制消息
    uint32_t offset = current * mq->max_size;
    memcpy(msg, mq->buffer + offset, size);

    // 更新队列状态
    if (current == mq->head) {
        mq->head = (mq->head + 1) % mq->max_msgs;
    } else {
        // 移动消息
        uint32_t move_start = current;
        uint32_t move_end = mq->head;
        while (move_start != move_end) {
            uint32_t prev = (move_start - 1 + mq->max_msgs) % mq->max_msgs;
            memcpy(mq->buffer + move_start * mq->max_size,
                   mq->buffer + prev * mq->max_size,
                   mq->max_size);
            move_start = prev;
        }
        mq->head = (mq->head + 1) % mq->max_msgs;
    }

    mq->msg_count--;
    ipc_stats.msg_receives++;

    // 通知等待的发送者
    condition_signal(&mq->not_full);

    mutex_unlock(&mq->lock);
    return 0;
} 