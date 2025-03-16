#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>

// 任务状态定义
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_TERMINATED
} task_state_t;

// 任务优先级定义
typedef enum {
    TASK_PRIORITY_IDLE = 0,
    TASK_PRIORITY_LOW,
    TASK_PRIORITY_NORMAL,
    TASK_PRIORITY_HIGH,
    TASK_PRIORITY_REALTIME
} task_priority_t;

// 任务上下文结构
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t cpsr;
} task_context_t;

// 任务控制块
typedef struct task_struct {
    task_context_t context;           // 任务上下文
    uint32_t *stack;                  // 栈指针
    uint32_t stack_size;              // 栈大小
    uint8_t priority;                 // 任务优先级
    uint8_t state;                    // 任务状态
    uint32_t time_slice;              // 时间片
    uint32_t ticks_remaining;         // 剩余时间片
    uint32_t total_ticks;             // 总运行时间
    char name[32];                    // 任务名称
    struct task_struct *next;         // 链表下一个节点
} task_t;

// 任务管理函数
void task_init(void);
task_t *task_create(const char *name, void (*entry)(void), uint8_t priority, uint32_t stack_size);
void task_delete(task_t *task);
void task_suspend(task_t *task);
void task_resume(task_t *task);
void task_yield(void);
void task_sleep(uint32_t ms);
void task_set_priority(task_t *task, uint8_t priority);
task_t *task_get_current(void);
void task_schedule(void);

// 系统任务相关常量
#define MAX_TASKS           32
#define DEFAULT_STACK_SIZE  4096
#define IDLE_TASK_STACK_SIZE 1024

#endif 