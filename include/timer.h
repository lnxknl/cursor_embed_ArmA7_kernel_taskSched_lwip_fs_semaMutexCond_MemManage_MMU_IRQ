#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>

void timer_init(void);
uint32_t timer_get_ticks(void);
void timer_delay_ms(uint32_t ms);
void timer_set_interval(uint32_t interval_ms);

#endif 