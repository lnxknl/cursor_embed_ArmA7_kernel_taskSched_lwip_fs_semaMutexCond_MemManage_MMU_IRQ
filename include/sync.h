#ifndef __SYNC_H__
#define __SYNC_H__

#include "task.h"
#include <stdint.h>

// 互斥量结构
typedef struct mutex {
    uint32_t locked;           // 锁定状态
    task_t *owner;            // 当前持有者
    task_t *waiting_tasks;    // 等待任务队列
    uint32_t recursive_count; // 递归锁定计数
    const char *name;         // 互斥量名称
} mutex_t;

// 信号量结构
typedef struct semaphore {
    int32_t count;           // 信号量计数
    task_t *waiting_tasks;   // 等待任务队列
    const char *name;        // 信号量名称
} semaphore_t;

// 条件变量结构
typedef struct condition {
    task_t *waiting_tasks;   // 等待任务队列
    const char *name;        // 条件变量名称
} condition_t;

// 读写锁结构
typedef struct rwlock {
    mutex_t mutex;           // 互斥量
    condition_t readers;     // 读者条件变量
    condition_t writers;     // 写者条件变量
    int32_t readers_count;   // 当前读者数量
    bool writer_active;      // 是否有写者活动
    task_t *writer_owner;    // 当前写者
    const char *name;        // 读写锁名称
} rwlock_t;

// 自旋锁结构
typedef struct spinlock {
    uint32_t locked;         // 锁定状态
    const char *name;        // 自旋锁名称
} spinlock_t;

// 互斥量函数
void mutex_init(mutex_t *mutex, const char *name);
void mutex_lock(mutex_t *mutex);
bool mutex_trylock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
bool mutex_is_locked(mutex_t *mutex);

// 递归互斥量函数
void recursive_mutex_init(mutex_t *mutex, const char *name);
void recursive_mutex_lock(mutex_t *mutex);
bool recursive_mutex_trylock(mutex_t *mutex);
void recursive_mutex_unlock(mutex_t *mutex);

// 信号量函数
void semaphore_init(semaphore_t *sem, int32_t initial_count, const char *name);
void semaphore_wait(semaphore_t *sem);
bool semaphore_trywait(semaphore_t *sem);
void semaphore_post(semaphore_t *sem);
int32_t semaphore_get_count(semaphore_t *sem);

// 条件变量函数
void condition_init(condition_t *cond, const char *name);
void condition_wait(condition_t *cond, mutex_t *mutex);
bool condition_timedwait(condition_t *cond, mutex_t *mutex, uint32_t timeout_ms);
void condition_signal(condition_t *cond);
void condition_broadcast(condition_t *cond);

// 读写锁函数
void rwlock_init(rwlock_t *rwlock, const char *name);
void rwlock_read_lock(rwlock_t *rwlock);
bool rwlock_read_trylock(rwlock_t *rwlock);
void rwlock_read_unlock(rwlock_t *rwlock);
void rwlock_write_lock(rwlock_t *rwlock);
bool rwlock_write_trylock(rwlock_t *rwlock);
void rwlock_write_unlock(rwlock_t *rwlock);

// 自旋锁函数
void spinlock_init(spinlock_t *spinlock, const char *name);
void spinlock_lock(spinlock_t *spinlock);
bool spinlock_trylock(spinlock_t *spinlock);
void spinlock_unlock(spinlock_t *spinlock);

// 调试和统计功能
typedef struct sync_stats {
    uint32_t mutex_contentions;     // 互斥量竞争次数
    uint32_t sem_contentions;       // 信号量竞争次数
    uint32_t cond_contentions;      // 条件变量竞争次数
    uint32_t rwlock_contentions;    // 读写锁竞争次数
    uint32_t spin_contentions;      // 自旋锁竞争次数
} sync_stats_t;

void sync_get_stats(sync_stats_t *stats);
void sync_reset_stats(void);

#endif 