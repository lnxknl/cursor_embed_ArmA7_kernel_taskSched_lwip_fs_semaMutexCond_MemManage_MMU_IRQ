#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>

void interrupt_init(void);
void interrupt_enable(uint32_t interrupt_id);
void interrupt_disable(uint32_t interrupt_id);
void interrupt_register_handler(uint32_t interrupt_id, void (*handler)(void));
void interrupt_set_priority(uint32_t interrupt_id, uint8_t priority);
void interrupt_set_target(uint32_t interrupt_id, uint8_t cpu_mask);

#endif 