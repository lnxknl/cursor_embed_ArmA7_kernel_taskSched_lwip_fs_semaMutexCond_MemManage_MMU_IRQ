#include "timer.h"
#include "interrupt.h"
#include "scheduler.h"
#include <stdint.h>

// 在定时器中断处理函数中添加调度器tick处理
static void timer_irq_handler(void) {
    // 清除中断
    *(volatile uint32_t *)TIMER_INTCLR = 1;
    
    // 增加系统滴答计数
    system_ticks++;
    
    // 调度器tick处理
    scheduler_tick();
} 