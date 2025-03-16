#include "uart.h"

// UART 寄存器地址定义
#define UART_BASE       0x10009000
#define UART_DR         (*((volatile uint32_t *)(UART_BASE + 0x000)))
#define UART_FR         (*((volatile uint32_t *)(UART_BASE + 0x018)))
#define UART_IBRD       (*((volatile uint32_t *)(UART_BASE + 0x024)))
#define UART_FBRD       (*((volatile uint32_t *)(UART_BASE + 0x028)))
#define UART_LCRH       (*((volatile uint32_t *)(UART_BASE + 0x02C)))
#define UART_CR         (*((volatile uint32_t *)(UART_BASE + 0x030)))
#define UART_IMSC       (*((volatile uint32_t *)(UART_BASE + 0x038)))

void uart_init(void) {
    // 禁用 UART
    UART_CR = 0x0;
    
    // 设置波特率为 115200
    // 假设系统时钟为 24MHz
    UART_IBRD = 13;
    UART_FBRD = 1;
    
    // 配置为 8N1
    UART_LCRH = 0x60;
    
    // 启用 UART，发送和接收
    UART_CR = 0x301;
}

void uart_putc(char c) {
    // 等待直到 UART 准备好发送
    while(UART_FR & (1 << 5));
    UART_DR = c;
}

char uart_getc(void) {
    // 等待直到接收到数据
    while(UART_FR & (1 << 4));
    return UART_DR;
}

void uart_puts(const char *s) {
    while(*s) {
        uart_putc(*s++);
    }
} 