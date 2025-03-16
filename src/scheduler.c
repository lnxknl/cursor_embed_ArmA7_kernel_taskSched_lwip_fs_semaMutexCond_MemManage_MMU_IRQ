#include "scheduler.h"
#include "task.h"
#include "uart.h"

// 调度器状态
static scheduler_state_t scheduler_state = SCHEDULER_STOPPED;
static scheduler_policy_t scheduler_policy = SCHEDULER_POLICY_PRIORITY;

// 就绪队列（按优先级）
static task_t *ready_queue[TASK_PRIORITY_REALTIME + 1];

// 初始化调度器
void scheduler_init(void) {
    // 清空就绪队列
    for (int i = 0; i <= TASK_PRIORITY_REALTIME; i++) {
        ready_queue[i] = NULL;
    }
    
    scheduler_state = SCHEDULER_STOPPED;
}

// 启动调度器
void scheduler_start(void) {
    if (scheduler_state == SCHEDULER_STOPPED) {
        scheduler_state = SCHEDULER_RUNNING;
        task_schedule();  // 开始第一次调度
    }
}

// 停止调度器
void scheduler_stop(void) {
    scheduler_state = SCHEDULER_STOPPED;
}

// 设置调度策略
void scheduler_set_policy(scheduler_policy_t policy) {
    scheduler_policy = policy;
}

// 获取当前调度策略
scheduler_policy_t scheduler_get_policy(void) {
    return scheduler_policy;
}

// 将任务添加到就绪队列
static void add_to_ready_queue(task_t *task) {
    if (!task || task->state != TASK_READY) {
        return;
    }

    task_t **queue = &ready_queue[task->priority];
    task->next = *queue;
    *queue = task;
}

// 从就绪队列中移除任务
static void remove_from_ready_queue(task_t *task) {
    if (!task) {
        return;
    }

    task_t **queue = &ready_queue[task->priority];
    if (*queue == task) {
        *queue = task->next;
    } else {
        task_t *current = *queue;
        while (current && current->next != task) {
            current = current->next;
        }
        if (current) {
            current->next = task->next;
        }
    }
    task->next = NULL;
}

// 获取最高优先级的就绪任务
static task_t *get_highest_priority_task(void) {
    for (int i = TASK_PRIORITY_REALTIME; i >= TASK_PRIORITY_IDLE; i--) {
        if (ready_queue[i]) {
            return ready_queue[i];
        }
    }
    return NULL;
}

// 调度器tick处理
void scheduler_tick(void) {
    task_t *current = task_get_current();
    
    if (!current || scheduler_state != SCHEDULER_RUNNING) {
        return;
    }

    // 更新任务统计信息
    current->total_ticks++;

    // 处理睡眠任务
    if (current->state == TASK_BLOCKED) {
        if (--current->ticks_remaining == 0) {
            current->state = TASK_READY;
            add_to_ready_queue(current);
        }
    }
    // 处理时间片
    else if (scheduler_policy == SCHEDULER_POLICY_ROUND_ROBIN) {
        if (--current->ticks_remaining == 0) {
            current->ticks_remaining = current->time_slice;
            current->state = TASK_READY;
            remove_from_ready_queue(current);
            add_to_ready_queue(current);
            scheduler_yield();
        }
    }
}

// 触发调度
void scheduler_yield(void) {
    task_t *current = task_get_current();
    
    if (current && current->state == TASK_RUNNING) {
        current->state = TASK_READY;
        add_to_ready_queue(current);
    }
    
    task_schedule();
}

// 获取下一个要运行的任务
task_t *scheduler_next_task(void) {
    task_t *next = NULL;
    
    if (scheduler_state != SCHEDULER_RUNNING) {
        return NULL;
    }

    // 根据调度策略选择下一个任务
    if (scheduler_policy == SCHEDULER_POLICY_PRIORITY) {
        next = get_highest_priority_task();
    } else {  // SCHEDULER_POLICY_ROUND_ROBIN
        task_t *current = task_get_current();
        if (current && current->next) {
            next = current->next;
        } else {
            next = ready_queue[current ? current->priority : TASK_PRIORITY_IDLE];
        }
    }

    // 如果没有就绪任务，运行空闲任务
    if (!next) {
        next = idle_task;
    }

    // 更新任务状态
    if (next) {
        remove_from_ready_queue(next);
        next->state = TASK_RUNNING;
    }

    return next;
} 