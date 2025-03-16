#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "task.h"

// 调度器状态
typedef enum {
    SCHEDULER_RUNNING,
    SCHEDULER_STOPPED
} scheduler_state_t;

// 调度策略
typedef enum {
    SCHEDULER_POLICY_ROUND_ROBIN,
    SCHEDULER_POLICY_PRIORITY
} scheduler_policy_t;

// 调度器初始化
void scheduler_init(void);

// 启动调度器
void scheduler_start(void);

// 停止调度器
void scheduler_stop(void);

// 设置调度策略
void scheduler_set_policy(scheduler_policy_t policy);

// 获取当前调度策略
scheduler_policy_t scheduler_get_policy(void);

// 调度器tick处理
void scheduler_tick(void);

// 触发调度
void scheduler_yield(void);

// 获取下一个要运行的任务
task_t *scheduler_next_task(void);

#endif 