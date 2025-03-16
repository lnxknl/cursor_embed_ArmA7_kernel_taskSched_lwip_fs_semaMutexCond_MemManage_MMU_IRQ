#include "timer.h"
#include "interrupt.h"
#include <stdint.h>

// 定时器寄存器定义
#define TIMER_BASE       0x1C110000
#define TIMER_LOAD      (TIMER_BASE + 0x00)
#define TIMER_VALUE     (TIMER_BASE + 0x04)
#define TIMER_CONTROL   (TIMER_BASE + 0x08)
#define TIMER_INTCLR    (TIMER_BASE + 0x0C)
#define TIMER_RIS       (TIMER_BASE + 0x10)
#define TIMER_MIS       (TIMER_BASE + 0x14)
#define TIMER_BGLOAD    (TIMER_BASE + 0x18)

// 定时器控制寄存器位
#define TIMER_CTRL_ENABLE    (1 << 7)
#define TIMER_CTRL_PERIODIC  (1 << 6)
#define TIMER_CTRL_INTEN     (1 << 5)
#define TIMER_CTRL_32BIT     (1 << 1)
#define TIMER_CTRL_ONESHOT   (1 << 0)

// 定时器中断ID
#define TIMER_IRQ_ID        36

// 系统时钟频率（假设为24MHz）
#define SYSTEM_CLOCK_HZ     24000000

// 系统滴答计数器
static volatile uint32_t system_ticks = 0;

// 定时器中断处理函数
static void timer_irq_handler(void) {
    // 清除中断
    *(volatile uint32_t *)TIMER_INTCLR = 1;
    
    // 增加系统滴答计数
    system_ticks++;
}

void timer_init(void) {
    // 停止定时器
    *(volatile uint32_t *)TIMER_CONTROL = 0;
    
    // 设置定时器加载值（1ms中断）
    *(volatile uint32_t *)TIMER_LOAD = SYSTEM_CLOCK_HZ / 1000;
    
    // 配置定时器
    *(volatile uint32_t *)TIMER_CONTROL = TIMER_CTRL_ENABLE |
                                         TIMER_CTRL_PERIODIC |
                                         TIMER_CTRL_INTEN |
                                         TIMER_CTRL_32BIT;
    
    // 注册中断处理函数
    interrupt_register_handler(TIMER_IRQ_ID, timer_irq_handler);
    
    // 设置中断优先级
    interrupt_set_priority(TIMER_IRQ_ID, 0);
    
    // 启用定时器中断
    interrupt_enable(TIMER_IRQ_ID);
}

uint32_t timer_get_ticks(void) {
    return system_ticks;
}

void timer_delay_ms(uint32_t ms) {
    uint32_t target = system_ticks + ms;
    while (system_ticks < target);
}

void timer_set_interval(uint32_t interval_ms) {
    // 停止定时器
    *(volatile uint32_t *)TIMER_CONTROL = 0;
    
    // 设置新的加载值
    *(volatile uint32_t *)TIMER_LOAD = (SYSTEM_CLOCK_HZ / 1000) * interval_ms;
    
    // 重新启动定时器
    *(volatile uint32_t *)TIMER_CONTROL = TIMER_CTRL_ENABLE |
                                         TIMER_CTRL_PERIODIC |
                                         TIMER_CTRL_INTEN |
                                         TIMER_CTRL_32BIT;
} 