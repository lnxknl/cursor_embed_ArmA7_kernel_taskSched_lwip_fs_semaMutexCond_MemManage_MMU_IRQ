#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);

#endif 