#include "task.h"
#include "scheduler.h"
#include "mmu.h"
#include "interrupt.h"
#include "timer.h"
#include "uart.h"

// 示例任务1
static void task1(void) {
    while (1) {
        uart_puts("Task 1 running...\r\n");
        task_sleep(1000);
    }
}

// 示例任务2
static void task2(void) {
    while (1) {
        uart_puts("Task 2 running...\r\n");
        task_sleep(1500);
    }
}

// 示例任务3
static void task3(void) {
    while (1) {
        uart_puts("Task 3 running...\r\n");
        task_sleep(2000);
    }
}

// 系统初始化
void system_init(void) {
    // 初始化硬件
    uart_init();
    uart_puts("System initializing...\r\n");

    // 初始化MMU
    mmu_init();
    mmu_enable();
    uart_puts("MMU initialized\r\n");

    // 初始化中断系统
    interrupt_init();
    uart_puts("Interrupt system initialized\r\n");

    // 初始化定时器
    timer_init();
    uart_puts("Timer initialized\r\n");

    // 初始化任务系统
    task_init();
    uart_puts("Task system initialized\r\n");

    // 初始化调度器
    scheduler_init();
    scheduler_set_policy(SCHEDULER_POLICY_PRIORITY);
    uart_puts("Scheduler initialized\r\n");

    // 创建示例任务
    task_t *t1 = task_create("task1", task1, TASK_PRIORITY_NORMAL, DEFAULT_STACK_SIZE);
    task_t *t2 = task_create("task2", task2, TASK_PRIORITY_HIGH, DEFAULT_STACK_SIZE);
    task_t *t3 = task_create("task3", task3, TASK_PRIORITY_LOW, DEFAULT_STACK_SIZE);

    if (!t1 || !t2 || !t3) {
        uart_puts("Failed to create tasks!\r\n");
        while(1);
    }

    uart_puts("Tasks created\r\n");
    uart_puts("System initialization complete!\r\n");

    // 启动调度器
    scheduler_start();
} 