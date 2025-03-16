#include "sync.h"
#include "interrupt.h"
#include "task.h"
#include "timer.h"

// 初始化条件变量
void condition_init(condition_t *cond, const char *name) {
    if (!cond) return;
    
    cond->waiting_tasks = NULL;
    cond->name = name;
}

// 等待条件变量
void condition_wait(condition_t *cond, mutex_t *mutex) {
    if (!cond || !mutex) return;
    
    interrupt_disable();
    
    // 增加竞争计数
    sync_stats.cond_contentions++;
    
    // 将当前任务添加到等待队列
    task_t *current = task_get_current();
    current->state = TASK_BLOCKED;
    add_to_wait_queue(&cond->waiting_tasks, current);
    
    // 释放互斥量
    mutex_unlock(mutex);
    
    interrupt_enable();
    task_yield();
    
    // 重新获取互斥量
    mutex_lock(mutex);
}

// 带超时的条件变量等待
bool condition_timedwait(condition_t *cond, mutex_t *mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return false;
    
    interrupt_disable();
    
    uint32_t start_time = timer_get_ticks();
    
    // 增加竞争计数
    sync_stats.cond_contentions++;
    
    // 将当前任务添加到等待队列
    task_t *current = task_get_current();
    current->state = TASK_BLOCKED;
    current->wake_time = start_time + timeout_ms;
    add_to_wait_queue(&cond->waiting_tasks, current);
    
    // 释放互斥量
    mutex_unlock(mutex);
    
    interrupt_enable();
    task_yield();
    
    // 重新获取互斥量
    mutex_lock(mutex);
    
    // 检查是否超时
    return (timer_get_ticks() - start_time) < timeout_ms;
}

// 唤醒一个等待任务
void condition_signal(condition_t *cond) {
    if (!cond) return;
    
    interrupt_disable();
    
    // 唤醒第一个等待任务
    task_t *waiting = remove_from_wait_queue(&cond->waiting_tasks);
    if (waiting) {
        waiting->state = TASK_READY;
        task_resume(waiting);
    }
    
    interrupt_enable();
}

// 唤醒所有等待任务
void condition_broadcast(condition_t *cond) {
    if (!cond) return;
    
    interrupt_disable();
    
    // 唤醒所有等待任务
    task_t *waiting;
    while ((waiting = remove_from_wait_queue(&cond->waiting_tasks))) {
        waiting->state = TASK_READY;
        task_resume(waiting);
    }
    
    interrupt_enable();
} 