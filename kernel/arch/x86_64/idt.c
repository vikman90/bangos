#include "idt.h"
#include "drivers/uart.h"
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

static void pic_disable(void) {
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x21));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    pic_disable();

    for (int i = 0; i < 32; i++) {
        uint8_t ist = (i == 8 || i == 13 || i == 14) ? 1 : 0; // Use IST1 for fault handling
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E, ist);
    }

    for (int i = 32; i < 256; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[7], 0x08, 0x8E, 0);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtp));
    kprintf("[IDT] 64-bit IDT initialized with 256 gates (PIC hardware IRQs disabled).\n");
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
