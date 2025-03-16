#include "mmu.h"
#include "uart.h"
#include "timer.h"

// 测试数据结构
typedef struct {
    uint32_t data[1024];  // 4KB 数据
} test_data_t;

// 在不同内存区域放置测试数据
static test_data_t normal_data __attribute__((section(".data")));
static test_data_t uncached_data __attribute__((section(".uncached_data")));

void benchmark_memory_access(void) {
    uint32_t start_time, end_time;
    int i;

    uart_puts("Starting memory benchmark...\r\n");

    // 测试普通缓存内存访问
    start_time = timer_get_ticks();
    for (i = 0; i < 1024; i++) {
        normal_data.data[i] = i;
    }
    end_time = timer_get_ticks();
    uart_puts("Cached memory write time: ");
    uart_print_dec(end_time - start_time);
    uart_puts(" ms\r\n");

    // 测试非缓存内存访问
    start_time = timer_get_ticks();
    for (i = 0; i < 1024; i++) {
        uncached_data.data[i] = i;
    }
    end_time = timer_get_ticks();
    uart_puts("Uncached memory write time: ");
    uart_print_dec(end_time - start_time);
    uart_puts(" ms\r\n");
}

int main(void) {
    // 初始化串口
    uart_init();
    uart_puts("System initializing...\r\n");

    // 初始化定时器
    timer_init();
    uart_puts("Timer initialized\r\n");

    // 初始化并启用 MMU
    mmu_init();
    mmu_enable();
    uart_puts("MMU initialized and enabled\r\n");

    // 运行内存访问测试
    test_memory_access();
    uart_puts("\r\nMemory access tests completed\r\n");

    // 运行内存性能基准测试
    benchmark_memory_access();
    uart_puts("\r\nMemory benchmark completed\r\n");

    // 主循环
    while (1) {
        timer_delay_ms(1000);
        uart_puts("System running...\r\n");
    }

    return 0;
} 