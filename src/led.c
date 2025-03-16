#include "led.h"

// LED 控制寄存器地址定义
#define LED_BASE        0x10010000
#define LED_CTRL        (*((volatile uint32_t *)(LED_BASE + 0x000)))

void led_init(void) {
    // 配置 LED 引脚为输出
    LED_CTRL &= ~(1 << 0);
}

void led_on(void) {
    LED_CTRL |= (1 << 0);
}

void led_off(void) {
    LED_CTRL &= ~(1 << 0);
} 