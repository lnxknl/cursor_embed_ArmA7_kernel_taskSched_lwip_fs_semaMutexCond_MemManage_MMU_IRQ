#include "sync.h"
#include "interrupt.h"
#include "task.h"

// 初始化信号量
void semaphore_init(semaphore_t *sem, int32_t initial_count, const char *name) {
    if (!sem) return;
    
    sem->count = initial_count;
    sem->waiting_tasks = NULL;
    sem->name = name;
}

// 等待信号量
void semaphore_wait(semaphore_t *sem) {
    if (!sem) return;
    
    interrupt_disable();
    
    if (sem->count > 0) {
        sem->count--;
        interrupt_enable();
        return;
    }
    
    // 增加竞争计数
    sync_stats.sem_contentions++;
    
    // 将当前任务添加到等待队列
    task_t *current = task_get_current();
    current->state = TASK_BLOCKED;
    add_to_wait_queue(&sem->waiting_tasks, current);
    
    interrupt_enable();
    task_yield();
}

// 尝试等待信号量
bool semaphore_trywait(semaphore_t *sem) {
    if (!sem) return false;
    
    interrupt_disable();
    
    if (sem->count > 0) {
        sem->count--;
        interrupt_enable();
        return true;
    }
    
    interrupt_enable();
    return false;
}

// 释放信号量
void semaphore_post(semaphore_t *sem) {
    if (!sem) return;
    
    interrupt_disable();
    
    // 如果有等待任务，唤醒第一个
    task_t *waiting = remove_from_wait_queue(&sem->waiting_tasks);
    if (waiting) {
        waiting->state = TASK_READY;
        task_resume(waiting);
    } else {
        sem->count++;
    }
    
    interrupt_enable();
}

// 获取信号量计数
int32_t semaphore_get_count(semaphore_t *sem) {
    if (!sem) return 0;
    return sem->count;
} 