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
    SCHEDULER_POLICY_ROUND_ROBIN,    // 轮转调度
    SCHEDULER_POLICY_PRIORITY,       // 优先级调度
    SCHEDULER_POLICY_REALTIME,       // 实时调度
    SCHEDULER_POLICY_MLFQ,          // 多级反馈队列
    SCHEDULER_POLICY_FAIR           // 完全公平调度
} scheduler_policy_t;

// 实时调度参数
typedef struct {
    uint32_t period;        // 周期（毫秒）
    uint32_t deadline;      // 截止时间
    uint32_t execution;     // 执行时间
    uint32_t next_release;  // 下次释放时间
} realtime_params_t;

// 公平调度参数
typedef struct {
    uint64_t vruntime;      // 虚拟运行时间
    uint32_t weight;        // 权重
    uint32_t min_granularity; // 最小调度粒度
} fair_params_t;

// MLFQ参数
#define MLFQ_QUEUE_COUNT 8
typedef struct {
    uint32_t current_queue;  // 当前队列
    uint32_t time_slice;     // 当前时间片
    uint32_t boost_ticks;    // 优先级提升计数器
} mlfq_params_t;

// 调度器统计信息
typedef struct {
    uint32_t context_switches;  // 上下文切换次数
    uint32_t preemptions;       // 抢占次数
    uint32_t scheduler_runs;    // 调度器运行次数
    uint32_t missed_deadlines;  // 错过的截止时间
} scheduler_stats_t;

// 调度器函数
void scheduler_init(void);
void scheduler_start(void);
void scheduler_stop(void);
void scheduler_set_policy(scheduler_policy_t policy);
scheduler_policy_t scheduler_get_policy(void);
void scheduler_tick(void);
void scheduler_yield(void);
task_t *scheduler_next_task(void);

// 实时调度函数
void scheduler_set_realtime_params(task_t *task, realtime_params_t *params);
int scheduler_check_schedulability(void);
void scheduler_update_deadlines(void);

// 公平调度函数
void scheduler_update_vruntime(task_t *task);
void scheduler_set_weight(task_t *task, uint32_t weight);
uint64_t scheduler_min_vruntime(void);

// MLFQ函数
void scheduler_mlfq_boost(void);
void scheduler_mlfq_update_queue(task_t *task);
void scheduler_mlfq_init_task(task_t *task);

// 统计信息函数
scheduler_stats_t *scheduler_get_stats(void);
void scheduler_reset_stats(void);

#endif 