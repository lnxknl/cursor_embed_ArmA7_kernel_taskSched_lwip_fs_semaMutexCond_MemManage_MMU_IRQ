#include "mmu.h"
#include <stdint.h>

// 一级页表基地址
static uint32_t *first_level_table = (uint32_t *)0x70004000;

// 二级页表基地址
static uint32_t *second_level_table = (uint32_t *)0x70008000;

// MMU 访问权限定义
#define AP_NO_ACCESS     0
#define AP_SYS_ACCESS    1
#define AP_USER_RO      2
#define AP_USER_RW      3

// 域访问控制
#define DOMAIN_NO_ACCESS 0
#define DOMAIN_CLIENT    1
#define DOMAIN_MANAGER   3

// 缓存和缓冲控制
#define CB_NONE         0
#define CB_WRITE_THROUGH 2
#define CB_WRITE_BACK   3

// 段描述符类型
#define SECTION_TYPE    2
#define SMALL_PAGE_TYPE 2

void mmu_init(void) {
    uint32_t i;
    
    // 清空一级页表
    for (i = 0; i < 4096; i++) {
        first_level_table[i] = 0;
    }

    // 设置域访问控制寄存器
    // 所有域设置为客户端
    uint32_t dacr = 0x55555555;
    __asm__ volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r" (dacr));

    // 映射前1MB为设备内存（非缓存）
    for (i = 0; i < 1024; i++) {
        first_level_table[i] = (i << 20) |  // 基地址
                              (SECTION_TYPE) |  // 段描述符
                              (0 << 3) |     // 非缓存
                              (0 << 2) |     // 非缓冲
                              (0 << 5) |     // 域0
                              (AP_SYS_ACCESS << 10); // 系统访问
    }

    // 映射RAM区域（启用缓存和缓冲）
    for (i = 1024; i < 2048; i++) {
        first_level_table[i] = (i << 20) |   // 基地址
                              (SECTION_TYPE) |   // 段描述符
                              (1 << 3) |      // 缓存
                              (1 << 2) |      // 缓冲
                              (0 << 5) |      // 域0
                              (AP_USER_RW << 10); // 用户可读写
    }

    // 设置转换表基地址寄存器
    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r" (first_level_table));

    // 使无效TLB
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));

    // 使无效指令缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));

    // 使无效数据缓存
    __asm__ volatile ("mcr p15, 0, %0, c7, c6, 0" : : "r" (0));
}

void mmu_enable(void) {
    uint32_t control;
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (control));
    control |= 1;  // 启用MMU
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (control));
}

void mmu_disable(void) {
    uint32_t control;
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (control));
    control &= ~1;  // 禁用MMU
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (control));
}

void mmu_map_section(uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t index = va >> 20;
    first_level_table[index] = (pa & 0xFFF00000) | flags | SECTION_TYPE;
    
    // 使无效TLB
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 1" : : "r" (va));
} 