#include "interrupt.h"
#include <stdint.h>

// GIC寄存器定义
#define GIC_DIST_BASE    0x1E001000
#define GIC_CPU_BASE     0x1E000100

// GIC分发器寄存器
#define GICD_CTLR        (GIC_DIST_BASE + 0x000)
#define GICD_TYPER       (GIC_DIST_BASE + 0x004)
#define GICD_IIDR        (GIC_DIST_BASE + 0x008)
#define GICD_IGROUPR     (GIC_DIST_BASE + 0x080)
#define GICD_ISENABLER   (GIC_DIST_BASE + 0x100)
#define GICD_ICENABLER   (GIC_DIST_BASE + 0x180)
#define GICD_ISPENDR     (GIC_DIST_BASE + 0x200)
#define GICD_ICPENDR     (GIC_DIST_BASE + 0x280)
#define GICD_IPRIORITYR  (GIC_DIST_BASE + 0x400)
#define GICD_ITARGETSR   (GIC_DIST_BASE + 0x800)
#define GICD_ICFGR       (GIC_DIST_BASE + 0xC00)

// GIC CPU接口寄存器
#define GICC_CTLR        (GIC_CPU_BASE + 0x00)
#define GICC_PMR         (GIC_CPU_BASE + 0x04)
#define GICC_BPR         (GIC_CPU_BASE + 0x08)
#define GICC_IAR         (GIC_CPU_BASE + 0x0C)
#define GICC_EOIR        (GIC_CPU_BASE + 0x10)

// 中断处理函数指针数组
static void (*interrupt_handlers[1020])(void);

// 读写寄存器的辅助函数
static inline uint32_t read_reg(uint32_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void write_reg(uint32_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
}

void interrupt_init(void) {
    uint32_t i;

    // 初始化中断处理函数数组
    for (i = 0; i < 1020; i++) {
        interrupt_handlers[i] = NULL;
    }

    // 禁用分发器
    write_reg(GICD_CTLR, 0);

    // 设置中断优先级掩码
    write_reg(GICC_PMR, 0xFF);

    // 设置二进制点
    write_reg(GICC_BPR, 0);

    // 启用CPU接口
    write_reg(GICC_CTLR, 1);

    // 启用分发器
    write_reg(GICD_CTLR, 1);
}

void interrupt_enable(uint32_t interrupt_id) {
    uint32_t reg_offset = interrupt_id / 32;
    uint32_t bit_offset = interrupt_id % 32;
    uint32_t reg_addr = GICD_ISENABLER + (reg_offset * 4);
    
    write_reg(reg_addr, 1 << bit_offset);
}

void interrupt_disable(uint32_t interrupt_id) {
    uint32_t reg_offset = interrupt_id / 32;
    uint32_t bit_offset = interrupt_id % 32;
    uint32_t reg_addr = GICD_ICENABLER + (reg_offset * 4);
    
    write_reg(reg_addr, 1 << bit_offset);
}

void interrupt_register_handler(uint32_t interrupt_id, void (*handler)(void)) {
    if (interrupt_id < 1020) {
        interrupt_handlers[interrupt_id] = handler;
    }
}

void interrupt_set_priority(uint32_t interrupt_id, uint8_t priority) {
    uint32_t reg_offset = interrupt_id / 4;
    uint32_t byte_offset = interrupt_id % 4;
    uint32_t reg_addr = GICD_IPRIORITYR + (reg_offset * 4);
    
    uint32_t value = read_reg(reg_addr);
    value &= ~(0xFF << (byte_offset * 8));
    value |= priority << (byte_offset * 8);
    write_reg(reg_addr, value);
}

void interrupt_set_target(uint32_t interrupt_id, uint8_t cpu_mask) {
    uint32_t reg_offset = interrupt_id / 4;
    uint32_t byte_offset = interrupt_id % 4;
    uint32_t reg_addr = GICD_ITARGETSR + (reg_offset * 4);
    
    uint32_t value = read_reg(reg_addr);
    value &= ~(0xFF << (byte_offset * 8));
    value |= cpu_mask << (byte_offset * 8);
    write_reg(reg_addr, value);
}

void irq_handler(void) {
    // 读取中断确认寄存器
    uint32_t iar = read_reg(GICC_IAR);
    uint32_t interrupt_id = iar & 0x3FF;

    // 检查是否是有效的中断ID
    if (interrupt_id < 1020) {
        // 调用注册的处理函数
        if (interrupt_handlers[interrupt_id]) {
            interrupt_handlers[interrupt_id]();
        }
    }

    // 写中断结束寄存器
    write_reg(GICC_EOIR, iar);
} 