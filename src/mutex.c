#include "sync.h"
#include "interrupt.h"
#include "task.h"
#include <string.h>

// 全局统计信息
static sync_stats_t sync_stats;

// 初始化互斥量
void mutex_init(mutex_t *mutex, const char *name) {
    if (!mutex) return;
    
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->waiting_tasks = NULL;
    mutex->recursive_count = 0;
    mutex->name = name;
}

// 将任务添加到等待队列
static void add_to_wait_queue(task_t **queue, task_t *task) {
    task->next_wait = NULL;
    if (!*queue) {
        *queue = task;
        return;
    }
    
    task_t *current = *queue;
    while (current->next_wait) {
        current = current->next_wait;
    }
    current->next_wait = task;
}

// 从等待队列中移除任务
static task_t *remove_from_wait_queue(task_t **queue) {
    if (!*queue) return NULL;
    
    task_t *task = *queue;
    *queue = task->next_wait;
    task->next_wait = NULL;
    return task;
}

// 锁定互斥量
void mutex_lock(mutex_t *mutex) {
    if (!mutex) return;
    
    interrupt_disable();
    
    task_t *current = task_get_current();
    
    // 如果互斥量未被锁定
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = current;
        interrupt_enable();
        return;
    }
    
    // 互斥量已被锁定，增加竞争计数
    sync_stats.mutex_contentions++;
    
    // 将当前任务添加到等待队列
    current->state = TASK_BLOCKED;
    add_to_wait_queue(&mutex->waiting_tasks, current);
    
    interrupt_enable();
    task_yield();  // 让出CPU
}

// 尝试锁定互斥量
bool mutex_trylock(mutex_t *mutex) {
    if (!mutex) return false;
    
    interrupt_disable();
    
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = task_get_current();
        interrupt_enable();
        return true;
    }
    
    interrupt_enable();
    return false;
}

// 解锁互斥量
void mutex_unlock(mutex_t *mutex) {
    if (!mutex) return;
    
    interrupt_disable();
    
    task_t *current = task_get_current();
    
    // 检查是否是所有者
    if (mutex->owner != current) {
        interrupt_enable();
        return;
    }
    
    // 如果有等待任务，唤醒第一个
    task_t *waiting = remove_from_wait_queue(&mutex->waiting_tasks);
    if (waiting) {
        mutex->owner = waiting;
        waiting->state = TASK_READY;
        task_resume(waiting);
    } else {
        mutex->locked = 0;
        mutex->owner = NULL;
    }
    
    interrupt_enable();
}

// 初始化递归互斥量
void recursive_mutex_init(mutex_t *mutex, const char *name) {
    mutex_init(mutex, name);
}

// 锁定递归互斥量
void recursive_mutex_lock(mutex_t *mutex) {
    if (!mutex) return;
    
    interrupt_disable();
    
    task_t *current = task_get_current();
    
    // 如果是当前所有者，增加递归计数
    if (mutex->owner == current) {
        mutex->recursive_count++;
        interrupt_enable();
        return;
    }
    
    // 否则按普通互斥量处理
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = current;
        mutex->recursive_count = 1;
        interrupt_enable();
        return;
    }
    
    sync_stats.mutex_contentions++;
    current->state = TASK_BLOCKED;
    add_to_wait_queue(&mutex->waiting_tasks, current);
    
    interrupt_enable();
    task_yield();
}

// 解锁递归互斥量
void recursive_mutex_unlock(mutex_t *mutex) {
    if (!mutex) return;
    
    interrupt_disable();
    
    task_t *current = task_get_current();
    
    // 检查是否是所有者
    if (mutex->owner != current) {
        interrupt_enable();
        return;
    }
    
    // 减少递归计数
    if (--mutex->recursive_count > 0) {
        interrupt_enable();
        return;
    }
    
    // 完全解锁
    task_t *waiting = remove_from_wait_queue(&mutex->waiting_tasks);
    if (waiting) {
        mutex->owner = waiting;
        mutex->recursive_count = 1;
        waiting->state = TASK_READY;
        task_resume(waiting);
    } else {
        mutex->locked = 0;
        mutex->owner = NULL;
        mutex->recursive_count = 0;
    }
    
    interrupt_enable();
} 