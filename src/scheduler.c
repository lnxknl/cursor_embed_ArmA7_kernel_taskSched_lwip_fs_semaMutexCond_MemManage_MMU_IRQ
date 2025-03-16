#include "scheduler.h"
#include "task.h"
#include <stdint.h>

// 调度器状态
static scheduler_state_t scheduler_state = SCHEDULER_STOPPED;
static scheduler_policy_t current_policy = SCHEDULER_POLICY_FAIR;
static scheduler_stats_t stats = {0};

// 初始化调度器
void scheduler_init(void) {
    scheduler_state = SCHEDULER_STOPPED;
    memset(&stats, 0, sizeof(stats));

    // 初始化各个调度器
    scheduler_mlfq_init();
    scheduler_fair_init();
}

// 启动调度器
void scheduler_start(void) {
    if (scheduler_state == SCHEDULER_STOPPED) {
        scheduler_state = SCHEDULER_RUNNING;
        stats.scheduler_runs++;
        task_schedule();
    }
}

// 停止调度器
void scheduler_stop(void) {
    scheduler_state = SCHEDULER_STOPPED;
}

// 设置调度策略
void scheduler_set_policy(scheduler_policy_t policy) {
    if (policy != current_policy) {
        // 保存当前任务状态
        task_t *current = task_get_current();
        if (current) {
            switch (current_policy) {
                case SCHEDULER_POLICY_MLFQ:
                    scheduler_mlfq_update_queue(current);
                    break;
                case SCHEDULER_POLICY_FAIR:
                    scheduler_update_vruntime(current);
                    break;
                case SCHEDULER_POLICY_REALTIME:
                    scheduler_update_deadlines();
                    break;
            }
        }

        current_policy = policy;
    }
}

// 调度器tick处理
void scheduler_tick(void) {
    if (scheduler_state != SCHEDULER_RUNNING) {
        return;
    }

    stats.scheduler_runs++;

    switch (current_policy) {
        case SCHEDULER_POLICY_MLFQ:
            scheduler_mlfq_tick();
            break;
        case SCHEDULER_POLICY_FAIR:
            scheduler_fair_tick();
            break;
        case SCHEDULER_POLICY_REALTIME:
            scheduler_rt_tick();
            break;
    }
}

// 获取下一个要运行的任务
task_t *scheduler_next_task(void) {
    task_t *next = NULL;

    switch (current_policy) {
        case SCHEDULER_POLICY_MLFQ:
            next = scheduler_mlfq_next();
            break;
        case SCHEDULER_POLICY_FAIR:
            next = scheduler_pick_next_fair();
            break;
        case SCHEDULER_POLICY_REALTIME:
            next = scheduler_rt_next();
            break;
    }

    if (next) {
        stats.context_switches++;
        if (task_get_current() && task_get_current()->state == TASK_RUNNING) {
            stats.preemptions++;
        }
    }

    return next;
}

// 获取调度器统计信息
scheduler_stats_t *scheduler_get_stats(void) {
    return &stats;
}

// 重置统计信息
void scheduler_reset_stats(void) {
    memset(&stats, 0, sizeof(stats));
} 
// 创建实时任务
realtime_params_t rt_params = {
    .period = 100,      // 100ms周期
    .deadline = 100,    // 100ms截止时间
    .execution = 20     // 20ms执行时间
};
task_t *rt_task = task_create("rt_task", rt_task_entry, TASK_PRIORITY_REALTIME, DEFAULT_STACK_SIZE);
scheduler_set_realtime_params(rt_task, &rt_params);

// 创建普通任务（使用CFS）
task_t *normal_task = task_create("normal_task", normal_task_entry, TASK_PRIORITY_NORMAL, DEFAULT_STACK_SIZE);
scheduler_set_weight(normal_task, 1024);  // 设置默认权重

// 创建MLFQ任务
task_t *mlfq_task = task_create("mlfq_task", mlfq_task_entry, TASK_PRIORITY_NORMAL, DEFAULT_STACK_SIZE);
scheduler_mlfq_init_task(mlfq_task);

// 切换调度策略
scheduler_set_policy(SCHEDULER_POLICY_REALTIME);  // 使用实时调度
scheduler_set_policy(SCHEDULER_POLICY_FAIR);      // 使用完全公平调度
scheduler_set_policy(SCHEDULER_POLICY_MLFQ);      // 使用多级反馈队列