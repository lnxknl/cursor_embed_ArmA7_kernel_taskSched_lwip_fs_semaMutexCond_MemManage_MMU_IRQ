#include "scheduler.h"
#include "task.h"
#include <stdint.h>

// MLFQ队列结构
typedef struct {
    task_t *head;
    task_t *tail;
    uint32_t time_quantum;
    uint32_t task_count;
} mlfq_queue_t;

// MLFQ全局变量
static mlfq_queue_t queues[MLFQ_QUEUE_COUNT];
static uint32_t boost_period = 100;  // 优先级提升周期
static uint32_t boost_counter = 0;

// 初始化MLFQ
void scheduler_mlfq_init(void) {
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        queues[i].head = NULL;
        queues[i].tail = NULL;
        queues[i].time_quantum = (1 << i) * BASE_QUANTUM;  // 指数增长的时间片
        queues[i].task_count = 0;
    }
}

// 将任务添加到队列
static void mlfq_enqueue(uint32_t queue_index, task_t *task) {
    if (queue_index >= MLFQ_QUEUE_COUNT || !task) {
        return;
    }

    mlfq_queue_t *queue = &queues[queue_index];
    task->next = NULL;

    if (!queue->head) {
        queue->head = queue->tail = task;
    } else {
        queue->tail->next = task;
        queue->tail = task;
    }

    queue->task_count++;
}

// 从队列中移除任务
static task_t *mlfq_dequeue(uint32_t queue_index) {
    if (queue_index >= MLFQ_QUEUE_COUNT) {
        return NULL;
    }

    mlfq_queue_t *queue = &queues[queue_index];
    if (!queue->head) {
        return NULL;
    }

    task_t *task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    task->next = NULL;
    queue->task_count--;
    return task;
}

// 初始化任务的MLFQ参数
void scheduler_mlfq_init_task(task_t *task) {
    mlfq_params_t *params = malloc(sizeof(mlfq_params_t));
    if (!params) {
        return;
    }

    params->current_queue = 0;
    params->time_slice = queues[0].time_quantum;
    params->boost_ticks = 0;

    task->scheduler_data = params;
    mlfq_enqueue(0, task);
}

// 优先级提升
void scheduler_mlfq_boost(void) {
    // 将所有任务移动到最高优先级队列
    for (int i = 1; i < MLFQ_QUEUE_COUNT; i++) {
        while (queues[i].head) {
            task_t *task = mlfq_dequeue(i);
            mlfq_params_t *params = (mlfq_params_t *)task->scheduler_data;
            params->current_queue = 0;
            params->time_slice = queues[0].time_quantum;
            mlfq_enqueue(0, task);
        }
    }

    boost_counter = 0;
}

// 更新任务队列
void scheduler_mlfq_update_queue(task_t *task) {
    if (!task || !task->scheduler_data) {
        return;
    }

    mlfq_params_t *params = (mlfq_params_t *)task->scheduler_data;
    
    // 如果用完时间片，降低优先级
    if (params->time_slice == 0) {
        uint32_t next_queue = params->current_queue + 1;
        if (next_queue >= MLFQ_QUEUE_COUNT) {
            next_queue = MLFQ_QUEUE_COUNT - 1;
        }

        // 从当前队列移除
        task_t *prev = NULL;
        task_t *curr = queues[params->current_queue].head;
        while (curr && curr != task) {
            prev = curr;
            curr = curr->next;
        }

        if (curr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                queues[params->current_queue].head = curr->next;
            }
            if (queues[params->current_queue].tail == curr) {
                queues[params->current_queue].tail = prev;
            }
            queues[params->current_queue].task_count--;
        }

        // 添加到新队列
        params->current_queue = next_queue;
        params->time_slice = queues[next_queue].time_quantum;
        mlfq_enqueue(next_queue, task);
    }
}

// MLFQ调度器tick处理
void scheduler_mlfq_tick(void) {
    task_t *current = task_get_current();
    
    if (current && current->scheduler_data) {
        mlfq_params_t *params = (mlfq_params_t *)current->scheduler_data;
        
        // 减少时间片
        if (params->time_slice > 0) {
            params->time_slice--;
        }
        
        // 检查是否需要优先级提升
        if (++boost_counter >= boost_period) {
            scheduler_mlfq_boost();
        }
    }
}

// 获取下一个要运行的任务
task_t *scheduler_mlfq_next(void) {
    // 从最高优先级队列开始查找就绪任务
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        task_t *task = queues[i].head;
        while (task) {
            if (task->state == TASK_READY) {
                return task;
            }
            task = task->next;
        }
    }
    
    return NULL;
} 