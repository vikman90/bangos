#ifndef KERNEL_DRIVERS_QEMU_H
#define KERNEL_DRIVERS_QEMU_H

#include <stdint.h>

#define QEMU_DEBUG_EXIT_PORT 0xF4

// Exit QEMU immediately via isa-debugexit device
// Note: QEMU returns exit status ((code << 1) | 1) to the host
void qemu_exit(uint8_t code);

// Trigger QEMU ACPI poweroff (port 0x604)
void qemu_poweroff(void);

#endif // KERNEL_DRIVERS_QEMU_H
