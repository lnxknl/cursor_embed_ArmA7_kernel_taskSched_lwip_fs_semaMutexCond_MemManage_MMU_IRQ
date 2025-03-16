#include <stdint.h>
#include "uart.h"
#include "led.h"

void delay(volatile uint32_t count) {
    while(count--);
}

int main(void) {
    // 初始化 UART
    uart_init();
    // 初始化 LED
    led_init();
    
    uart_puts("Hello from ARM Cortex-A7!\r\n");
    
    while(1) {
        led_on();
        delay(1000000);
        led_off();
        delay(1000000);
        
        uart_puts("LED Toggle\r\n");
    }
    
    return 0;
} 