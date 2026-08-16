#include "pit.h"
#include "uart.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

volatile uint64_t pit_ticks = 0;

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    (void)mask1; (void)mask2;

    // Start initialization sequence in cascade mode
    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    // Set vector offsets: Master -> 0x20 (32), Slave -> 0x28 (40)
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // Configure cascade connection
    outb(PIC1_DATA, 0x04); // Slave PIC at IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02); // Cascade identity
    io_wait();

    // 8086/88 (MCS-80/85) mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Unmask IRQ0 (Timer) on Master PIC, mask all on Slave PIC
    outb(PIC1_DATA, 0xFE); // 11111110b -> only IRQ0 unmasked
    outb(PIC2_DATA, 0xFF);

    kprintf("[PIC] 8259 PIC remapped (Master: 0x20, Slave: 0x28), IRQ0 unmasked.\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pit_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 100;
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor > 65535) divisor = 65535;

    // Channel 0, lobyte/hibyte, Mode 3 (Square Wave), 16-bit binary
    outb(PIT_COMMAND, 0x36);
    io_wait();

    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    io_wait();

    pit_ticks = 0;
    kprintf("[PIT] 8254 PIT Channel 0 initialized at %u Hz (Divisor: %u).\n",
            frequency_hz, divisor);
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}
