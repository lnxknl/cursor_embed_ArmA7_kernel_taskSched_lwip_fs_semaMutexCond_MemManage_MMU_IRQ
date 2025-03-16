#ifndef __IPC_H__
#define __IPC_H__

#include "task.h"
#include "sync.h"
#include <stdint.h>
#include <stdbool.h>

// IPC 类型定义
typedef enum {
    IPC_TYPE_MSG_QUEUE,
    IPC_TYPE_SHARED_MEM,
    IPC_TYPE_PIPE
} ipc_type_t;

// IPC 权限
typedef struct {
    uint32_t uid;    // 用户ID
    uint32_t gid;    // 组ID
    uint32_t mode;   // 访问模式
} ipc_perm_t;

// 消息队列消息结构
typedef struct {
    long type;              // 消息类型
    uint32_t size;         // 消息大小
    uint8_t data[];        // 消息数据
} msg_t;

// 消息队列结构
typedef struct msg_queue {
    ipc_perm_t perm;           // 权限
    uint32_t msg_count;        // 消息数量
    uint32_t max_msgs;         // 最大消息数
    uint32_t max_size;         // 最大消息大小
    uint8_t *buffer;           // 消息缓冲区
    uint32_t head;             // 队列头
    uint32_t tail;             // 队列尾
    mutex_t lock;              // 互斥锁
    condition_t not_full;      // 非满条件变量
    condition_t not_empty;     // 非空条件变量
    struct msg_queue *next;    // 链表下一个节点
} msg_queue_t;

// 共享内存段结构
typedef struct shm_segment {
    ipc_perm_t perm;           // 权限
    uint32_t size;             // 段大小
    void *addr;                // 映射地址
    uint32_t ref_count;        // 引用计数
    mutex_t lock;              // 互斥锁
    struct shm_segment *next;  // 链表下一个节点
} shm_segment_t;

// 管道结构
typedef struct pipe {
    uint8_t *buffer;           // 缓冲区
    uint32_t size;             // 缓冲区大小
    uint32_t read_pos;         // 读位置
    uint32_t write_pos;        // 写位置
    uint32_t count;            // 数据计数
    mutex_t lock;              // 互斥锁
    condition_t not_full;      // 非满条件变量
    condition_t not_empty;     // 非空条件变量
    bool reader_closed;        // 读端关闭标志
    bool writer_closed;        // 写端关闭标志
    struct pipe *next;         // 链表下一个节点
} pipe_t;

// IPC 统计信息
typedef struct {
    uint32_t msg_queues;       // 消息队列数量
    uint32_t shm_segments;     // 共享内存段数量
    uint32_t pipes;            // 管道数量
    uint32_t msg_sends;        // 消息发送次数
    uint32_t msg_receives;     // 消息接收次数
    uint32_t shm_attaches;     // 共享内存附加次数
    uint32_t pipe_writes;      // 管道写入次数
    uint32_t pipe_reads;       // 管道读取次数
} ipc_stats_t;

// 消息队列函数
int msgq_create(key_t key, uint32_t max_msgs, uint32_t max_size);
int msgq_open(key_t key);
int msgq_send(int mqid, const msg_t *msg, uint32_t size, uint32_t timeout);
int msgq_receive(int mqid, msg_t *msg, uint32_t size, long type, uint32_t timeout);
int msgq_close(int mqid);
int msgq_delete(int mqid);

// 共享内存函数
int shm_create(key_t key, uint32_t size);
int shm_open(key_t key);
void *shm_attach(int shmid);
int shm_detach(void *addr);
int shm_close(int shmid);
int shm_delete(int shmid);

// 管道函数
int pipe_create(int pipefd[2]);
int pipe_write(int fd, const void *buf, uint32_t count);
int pipe_read(int fd, void *buf, uint32_t count);
int pipe_close(int fd);

// IPC 控制函数
int ipc_get_stats(ipc_stats_t *stats);
void ipc_reset_stats(void);

#endif 