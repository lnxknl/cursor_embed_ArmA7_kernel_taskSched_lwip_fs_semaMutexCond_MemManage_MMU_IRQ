#include "mmu.h"
#include "interrupt.h"
#include "timer.h"
#include "uart.h"
#include <stdint.h>

// LED控制寄存器（示例地址）
#define LED_CTRL (*((volatile uint32_t *)0x10010000))

void led_init(void) {
    LED_CTRL = 0;
}

void led_toggle(void) {
    LED_CTRL ^= 1;
}

int main(void) {
    // 初始化串口
    uart_init();
    uart_puts("System initializing...\r\n");

    // LED初始化
    led_init();
    uart_puts("LED initialized\r\n");

    // MMU已在启动代码中初始化
    uart_puts("MMU initialized\r\n");

    // 中断系统已在启动代码中初始化
    uart_puts("Interrupt system initialized\r\n");

    // 定时器已在启动代码中初始化
    uart_puts("Timer initialized\r\n");

    uart_puts("System initialization complete!\r\n");

    // 主循环
    while (1) {
        led_toggle();
        timer_delay_ms(500);  // 500ms延时
        uart_puts("LED toggled\r\n");
    }

    return 0;
} 