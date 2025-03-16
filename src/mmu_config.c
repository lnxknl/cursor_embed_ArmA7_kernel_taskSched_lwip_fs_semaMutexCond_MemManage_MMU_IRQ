#include "mmu.h"
#include "uart.h"
#include <stdint.h>

// MMU 配置常量
#define SECTION_SIZE               0x100000    // 1MB
#define TOTAL_SECTIONS            4096        // 4GB/1MB
#define PAGE_TABLE_BASE           0x70004000
#define SECOND_LEVEL_TABLE_BASE   0x70008000

// 内存区域定义
#define PERIPH_BASE               0x10000000
#define PERIPH_SIZE              (32 * SECTION_SIZE)  // 32MB
#define RAM_BASE                  0x70000000
#define RAM_SIZE                 (128 * SECTION_SIZE) // 128MB
#define USER_SPACE_BASE          0x80000000
#define USER_SPACE_SIZE          (256 * SECTION_SIZE) // 256MB

// MMU 控制位
#define MMU_SECTION              (0x2)        // Section descriptor
#define MMU_CACHEABLE           (1 << 3)     // C bit
#define MMU_BUFFERABLE         (1 << 2)     // B bit
#define MMU_ACCESS_RW          (0x3 << 10)  // AP bits
#define MMU_ACCESS_RO          (0x2 << 10)
#define MMU_DOMAIN            (0x0 << 5)    // Domain 0

// 页表条目类型
typedef enum {
    UNMAPPED,           // 未映射
    STRONGLY_ORDERED,   // 强序访问（无缓存）
    DEVICE,            // 设备内存
    NORMAL_CACHED,     // 普通内存（带缓存）
    NORMAL_UNCACHED    // 普通内存（无缓存）
} memory_type_t;

// 内存区域描述符
typedef struct {
    uint32_t virtual_addr;
    uint32_t physical_addr;
    uint32_t size;
    memory_type_t type;
    uint8_t access_permissions;
    uint8_t executable;
} memory_region_t;

// 测试用的共享内存区域
static volatile uint32_t *shared_memory = (volatile uint32_t *)0x80000000;

// 创建页表条目
static uint32_t create_section_entry(uint32_t physical_addr, memory_type_t type, uint8_t ap) {
    uint32_t entry = physical_addr & 0xFFF00000; // 基地址
    entry |= MMU_SECTION;                        // 段描述符
    entry |= MMU_DOMAIN;                         // 域

    switch (type) {
        case STRONGLY_ORDERED:
            // 无缓存，无缓冲
            break;
        case DEVICE:
            entry |= MMU_BUFFERABLE;             // 只启用缓冲
            break;
        case NORMAL_CACHED:
            entry |= MMU_CACHEABLE | MMU_BUFFERABLE; // 启用缓存和缓冲
            break;
        case NORMAL_UNCACHED:
            // 无缓存，无缓冲
            break;
        default:
            break;
    }

    entry |= (ap << 10);                        // 访问权限
    return entry;
}

// 配置内存区域
static void configure_memory_region(const memory_region_t *region) {
    uint32_t *page_table = (uint32_t *)PAGE_TABLE_BASE;
    uint32_t num_sections = region->size / SECTION_SIZE;
    uint32_t virt_section = region->virtual_addr / SECTION_SIZE;
    uint32_t phys_section = region->physical_addr / SECTION_SIZE;

    for (uint32_t i = 0; i < num_sections; i++) {
        page_table[virt_section + i] = create_section_entry(
            (phys_section + i) * SECTION_SIZE,
            region->type,
            region->access_permissions
        );
    }
}

// MMU 初始化
void mmu_init(void) {
    uint32_t *page_table = (uint32_t *)PAGE_TABLE_BASE;

    // 清空页表
    for (int i = 0; i < TOTAL_SECTIONS; i++) {
        page_table[i] = 0;
    }

    // 定义内存区域
    memory_region_t regions[] = {
        // 外设区域 - 强序访问
        {
            .virtual_addr = PERIPH_BASE,
            .physical_addr = PERIPH_BASE,
            .size = PERIPH_SIZE,
            .type = STRONGLY_ORDERED,
            .access_permissions = MMU_ACCESS_RW,
            .executable = 0
        },
        // RAM区域 - 带缓存
        {
            .virtual_addr = RAM_BASE,
            .physical_addr = RAM_BASE,
            .size = RAM_SIZE,
            .type = NORMAL_CACHED,
            .access_permissions = MMU_ACCESS_RW,
            .executable = 1
        },
        // 用户空间 - 带缓存，只读
        {
            .virtual_addr = USER_SPACE_BASE,
            .physical_addr = RAM_BASE + RAM_SIZE,
            .size = USER_SPACE_SIZE,
            .type = NORMAL_CACHED,
            .access_permissions = MMU_ACCESS_RO,
            .executable = 0
        }
    };

    // 配置所有内存区域
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        configure_memory_region(&regions[i]);
    }

    // 设置域访问控制
    uint32_t dacr = 0x1; // 域0设置为客户端
    __asm__ volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r" (dacr));

    // 设置转换表基地址
    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r" (page_table));

    // 使无效TLB
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));

    // 使无效指令缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));

    // 使无效数据缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c6, 0" : : "r" (0));
}

// 测试函数：访问不同类型的内存
void test_memory_access(void) {
    uart_puts("Starting memory access tests...\r\n");

    // 1. 测试外设访问（强序）
    volatile uint32_t *periph = (volatile uint32_t *)PERIPH_BASE;
    uart_puts("Testing peripheral access...\r\n");
    *periph = 0x12345678;
    uart_puts("Peripheral write successful\r\n");

    // 2. 测试RAM访问（带缓存）
    volatile uint32_t *ram = (volatile uint32_t *)RAM_BASE;
    uart_puts("Testing RAM access...\r\n");
    *ram = 0x87654321;
    if (*ram == 0x87654321) {
        uart_puts("RAM read/write successful\r\n");
    }

    // 3. 测试用户空间（只读）
    uart_puts("Testing user space access...\r\n");
    volatile uint32_t *user_space = (volatile uint32_t *)USER_SPACE_BASE;
    
    // 尝试读取（应该成功）
    uint32_t value = *user_space;
    uart_puts("User space read successful\r\n");

    // 尝试写入（应该触发异常）
    uart_puts("Attempting to write to read-only memory...\r\n");
    *user_space = 0x11111111;  // 这里应该触发数据访问异常
}

// 缓存操作函数
void cache_operations(void) {
    // 清理数据缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 0" : : "r" (0));
    
    // 清理并使无效数据缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c14, 0" : : "r" (0));
    
    // 使无效指令缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
    
    // 数据同步屏障
    __asm__ volatile ("dsb");
    
    // 指令同步屏障
    __asm__ volatile ("isb");
}

// 异常处理函数
void data_abort_handler(void) {
    uint32_t dfar, dfsr;
    
    // 读取故障地址寄存器
    __asm__ volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (dfar));
    
    // 读取故障状态寄存器
    __asm__ volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (dfsr));

    uart_puts("Data Abort Exception!\r\n");
    uart_puts("Fault Address: ");
    // 这里应该添加一个函数来打印十六进制值
    uart_puts("\r\n");
} 