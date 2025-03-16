#ifndef __MMU_H__
#define __MMU_H__

#include <stdint.h>

void mmu_init(void);
void mmu_enable(void);
void mmu_disable(void);
void mmu_map_section(uint32_t va, uint32_t pa, uint32_t flags);

// MMU 标志位定义
#define MMU_FLAG_CACHED      (1 << 3)
#define MMU_FLAG_BUFFERED    (1 << 2)
#define MMU_FLAG_ACCESS_USER (3 << 10)
#define MMU_FLAG_ACCESS_RO   (2 << 10)
#define MMU_FLAG_SECTION     (2)

#endif 