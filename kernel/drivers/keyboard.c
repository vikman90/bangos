#include "keyboard.h"
#include "uart.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',   0, ' '
};

void keyboard_init(void) {
    // Basic PS/2 status check
}

int keyboard_has_char(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 1) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        if (!(scancode & 0x80)) { // Make code
            if (scancode < sizeof(scancode_map) && scancode_map[scancode] != 0) {
                return scancode_map[scancode];
            }
        }
    }
    return 0;
}

char keyboard_getc(void) {
    while (1) {
        int ch = keyboard_has_char();
        if (ch > 0) return (char)ch;
        __asm__ volatile ("sti; pause; cli");
    }
}

int console_has_char(void) {
    return uart_has_data();
}

char console_getchar(void) {
    while (1) {
        if (uart_has_data()) {
            return uart_getc();
        }
        int kbd_ch = keyboard_has_char();
        if (kbd_ch > 0) {
            return (char)kbd_ch;
        }
        __asm__ volatile ("sti; pause; cli");
    }
}
