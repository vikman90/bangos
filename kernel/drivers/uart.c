#include "drivers/uart.h"
#include <stdarg.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void uart_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB
    outb(COM1_PORT + 0, 0x03); // Divisor 3 (38400 baud)
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(COM1_PORT + 2, 0x07); // Enable FIFO, 1-byte trigger threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

int uart_has_data(void) {
    return (inb(COM1_PORT + 5) & 1) != 0;
}

char uart_getc(void) {
    while (!uart_has_data());
    char c = (char)inb(COM1_PORT);
    if (c == '\r') {
        c = '\n';
    }
    return c;
}

void uart_putc(char c) {
    if (c == '\n') {
        while ((inb(COM1_PORT + 5) & 0x20) == 0);
        outb(COM1_PORT, '\r');
    }
    while ((inb(COM1_PORT + 5) & 0x20) == 0);
    outb(COM1_PORT, c);
}

void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

static void itoa(uint64_t val, char *buf, int base) {
    char temp[64];
    int i = 0;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (val > 0) {
        int rem = val % base;
        temp[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        val /= base;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;
        char buf[64];
        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                uart_puts(s ? s : "(null)");
                break;
            }
            case 'd': {
                int d = va_arg(args, int);
                if (d < 0) {
                    uart_putc('-');
                    d = -d;
                }
                itoa((uint64_t)d, buf, 10);
                uart_puts(buf);
                break;
            }
            case 'u': {
                unsigned int u = va_arg(args, unsigned int);
                itoa((uint64_t)u, buf, 10);
                uart_puts(buf);
                break;
            }
            case 'x': {
                unsigned int x = va_arg(args, unsigned int);
                itoa((uint64_t)x, buf, 16);
                uart_puts(buf);
                break;
            }
            case 'p': {
                uint64_t ptr = va_arg(args, uint64_t);
                itoa(ptr, buf, 16);
                uart_puts(buf);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                uart_putc(c);
                break;
            }
            case '%': {
                uart_putc('%');
                break;
            }
            default:
                uart_putc('%');
                uart_putc(*p);
                break;
        }
    }
    va_end(args);
}
