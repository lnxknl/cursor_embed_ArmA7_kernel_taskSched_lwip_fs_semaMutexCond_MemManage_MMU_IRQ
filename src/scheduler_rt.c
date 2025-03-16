#include "scheduler.h"
#include "task.h"
#include "timer.h"
#include <stdint.h>

// 实时任务链表
static task_t *rt_tasks = NULL;
static uint32_t rt_task_count = 0;

// EDF (Earliest Deadline First) 调度实现
static task_t *edf_schedule(void) {
    task_t *task = rt_tasks;
    task_t *earliest = NULL;
    uint32_t earliest_deadline = UINT32_MAX;

    while (task) {
        if (task->state == TASK_READY) {
            realtime_params_t *rt_params = (realtime_params_t *)task->scheduler_data;
            if (rt_params->deadline < earliest_deadline) {
                earliest_deadline = rt_params->deadline;
                earliest = task;
            }
        }
        task = task->next;
    }

    return earliest;
}

// Rate Monotonic 调度实现
static task_t *rm_schedule(void) {
    task_t *task = rt_tasks;
    task_t *highest = NULL;
    uint32_t highest_priority = UINT32_MAX;

    while (task) {
        if (task->state == TASK_READY) {
            realtime_params_t *rt_params = (realtime_params_t *)task->scheduler_data;
            if (rt_params->period < highest_priority) {
                highest_priority = rt_params->period;
                highest = task;
            }
        }
        task = task->next;
    }

    return highest;
}

// 设置实时参数
void scheduler_set_realtime_params(task_t *task, realtime_params_t *params) {
    if (!task || !params) {
        return;
    }

    // 分配实时参数结构
    realtime_params_t *rt_params = malloc(sizeof(realtime_params_t));
    if (!rt_params) {
        return;
    }

    // 复制参数
    memcpy(rt_params, params, sizeof(realtime_params_t));
    rt_params->next_release = timer_get_ticks() + params->period;

    // 设置任务的调度器数据
    task->scheduler_data = rt_params;

    // 添加到实时任务链表
    task->next = rt_tasks;
    rt_tasks = task;
    rt_task_count++;
}

// 检查可调度性（利用率测试）
int scheduler_check_schedulability(void) {
    task_t *task = rt_tasks;
    float total_utilization = 0.0f;
    uint32_t n = rt_task_count;

    // 计算总利用率
    while (task) {
        realtime_params_t *rt_params = (realtime_params_t *)task->scheduler_data;
        total_utilization += (float)rt_params->execution / rt_params->period;
        task = task->next;
    }

    // Liu & Layland 界限
    float bound = n * (powf(2.0f, 1.0f/n) - 1);
    
    return total_utilization <= bound;
}

// 更新截止时间
void scheduler_update_deadlines(void) {
    task_t *task = rt_tasks;
    uint32_t current_time = timer_get_ticks();

    while (task) {
        realtime_params_t *rt_params = (realtime_params_t *)task->scheduler_data;
        
        // 检查是否到达释放时间
        if (current_time >= rt_params->next_release) {
            // 更新下次释放时间
            rt_params->next_release += rt_params->period;
            
            // 更新截止时间
            rt_params->deadline = current_time + rt_params->period;
            
            // 如果任务被阻塞，设置为就绪状态
            if (task->state == TASK_BLOCKED) {
                task->state = TASK_READY;
            }
        }
        
        task = task->next;
    }
}

// 实时调度器tick处理
void scheduler_rt_tick(void) {
    task_t *current = task_get_current();
    
    // 更新截止时间
    scheduler_update_deadlines();
    
    if (current && current->scheduler_data) {
        realtime_params_t *rt_params = (realtime_params_t *)current->scheduler_data;
        
        // 检查是否超过执行时间
        if (++rt_params->execution_time >= rt_params->execution) {
            current->state = TASK_BLOCKED;
            scheduler_yield();
        }
        
        // 检查是否错过截止时间
        if (timer_get_ticks() > rt_params->deadline) {
            scheduler_stats.missed_deadlines++;
        }
    }
} 