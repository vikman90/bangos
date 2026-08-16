#include "idt.h"
#include "drivers/uart.h"
#include "drivers/pit.h"
#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern uint64_t isr_stub_table[32];
extern void isr_32(void);

static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].offset_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = ist;
    idt[num].type_attr   = flags;
    idt[num].zero        = 0;
}

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    pic_remap();
    pit_init(100);

    for (int i = 0; i < 32; i++) {
        uint8_t ist = (i == 8 || i == 13 || i == 14) ? 1 : 0; // Use IST1 for fault handling
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E, ist);
    }

    // Gate 32: Timer IRQ 0
    idt_set_gate(32, (uint64_t)isr_32, 0x08, 0x8E, 0);

    for (int i = 33; i < 256; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[7], 0x08, 0x8E, 0);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtp));
    kprintf("[IDT] 64-bit IDT initialized with 256 gates (Timer IRQ0 on Vector 32 enabled).\n");
}

void exception_handler(exception_frame_t *frame) {
    uart_putc('E');
    uart_putc('X');
    uart_putc('C');
    uart_putc(':');
    uart_putc('0' + (frame->vec_num % 10));
    uart_putc('\n');

    uint64_t cr2 = read_cr2();
    kprintf("\n======================================================\n");
    kprintf(" [CPU EXCEPTION #%u] ErrorCode=%p\n", (uint32_t)frame->vec_num, frame->error_code);
    kprintf(" RIP=%p  RSP=%p  CR2=%p\n", frame->rip, frame->rsp, cr2);
    kprintf(" RAX=%p  RBX=%p  RCX=%p  RDX=%p\n", frame->rax, frame->rbx, frame->rcx, frame->rdx);
    kprintf(" RSI=%p  RDI=%p  RBP=%p\n", frame->rsi, frame->rdi, frame->rbp);
    kprintf("======================================================\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}
