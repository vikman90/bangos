#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

#define COM1_PORT 0x3F8

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
int  uart_has_data(void);
void uart_puts(const char *str);
void kprintf(const char *fmt, ...);

#endif /* UART_H */
