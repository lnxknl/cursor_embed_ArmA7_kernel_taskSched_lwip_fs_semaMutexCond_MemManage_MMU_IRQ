#include "task.h"
#include "scheduler.h"
#include "uart.h"
#include <string.h>

// 任务列表
static task_t task_list[MAX_TASKS];
static task_t *current_task = NULL;
static task_t *idle_task = NULL;
static uint32_t task_count = 0;

// 空闲任务
static void idle_task_entry(void) {
    while (1) {
        __asm__ volatile ("wfi");  // 等待中断
    }
}

// 初始化任务系统
void task_init(void) {
    // 清空任务列表
    memset(task_list, 0, sizeof(task_list));
    task_count = 0;
    current_task = NULL;

    // 创建空闲任务
    idle_task = task_create("idle", idle_task_entry, TASK_PRIORITY_IDLE, IDLE_TASK_STACK_SIZE);
    if (!idle_task) {
        uart_puts("Failed to create idle task!\r\n");
        while(1);
    }
}

// 创建新任务
task_t *task_create(const char *name, void (*entry)(void), uint8_t priority, uint32_t stack_size) {
    task_t *task = NULL;
    uint32_t *stack;

    // 检查是否达到最大任务数
    if (task_count >= MAX_TASKS) {
        return NULL;
    }

    // 分配任务控制块
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_list[i].state == TASK_TERMINATED) {
            task = &task_list[i];
            break;
        }
    }

    if (!task) {
        return NULL;
    }

    // 分配栈空间
    stack = (uint32_t *)malloc(stack_size);
    if (!stack) {
        return NULL;
    }

    // 初始化任务控制块
    memset(task, 0, sizeof(task_t));
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->priority = priority;
    task->state = TASK_READY;
    task->stack = stack;
    task->stack_size = stack_size;
    task->time_slice = 100; // 默认时间片100ms
    task->ticks_remaining = task->time_slice;
    task->total_ticks = 0;

    // 初始化任务上下文
    task->context.cpsr = 0x13;  // SVC模式
    task->context.pc = (uint32_t)entry;
    task->context.lr = (uint32_t)task_exit;
    task->context.sp = (uint32_t)(stack + stack_size/4 - 1);

    // 将任务添加到就绪队列
    task_count++;
    
    return task;
}

// 删除任务
void task_delete(task_t *task) {
    if (!task || task == idle_task) {
        return;
    }

    // 从调度队列中移除
    task->state = TASK_TERMINATED;
    free(task->stack);
    task_count--;

    // 如果删除的是当前任务，强制调度
    if (task == current_task) {
        task_yield();
    }
}

// 挂起任务
void task_suspend(task_t *task) {
    if (!task || task == idle_task) {
        return;
    }

    task->state = TASK_SUSPENDED;

    // 如果挂起当前任务，强制调度
    if (task == current_task) {
        task_yield();
    }
}

// 恢复任务
void task_resume(task_t *task) {
    if (!task) {
        return;
    }

    if (task->state == TASK_SUSPENDED) {
        task->state = TASK_READY;
    }
}

// 任务让出CPU
void task_yield(void) {
    scheduler_yield();
}

// 任务睡眠
void task_sleep(uint32_t ms) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
        current_task->ticks_remaining = ms;
        task_yield();
    }
}

// 设置任务优先级
void task_set_priority(task_t *task, uint8_t priority) {
    if (task && task != idle_task) {
        task->priority = priority;
    }
}

// 获取当前任务
task_t *task_get_current(void) {
    return current_task;
}

// 任务调度
void task_schedule(void) {
    task_t *next = scheduler_next_task();
    
    if (next != current_task) {
        task_t *prev = current_task;
        current_task = next;
        context_switch(prev, next);
    }
}

// 任务退出处理
static void task_exit(void) {
    task_delete(current_task);
}

// 上下文切换（在汇编中实现）
extern void context_switch(task_t *prev, task_t *next); 