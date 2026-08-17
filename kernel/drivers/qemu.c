#include "qemu.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void qemu_exit(uint8_t code) {
    outb(QEMU_DEBUG_EXIT_PORT, code);
}

void qemu_poweroff(void) {
    outw(0x604, 0x2000); // Standard QEMU ACPI shutdown port
}
