#include "gdt.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

static uint8_t gdt_bytes[sizeof(struct gdt_entry) * 5 + sizeof(struct tss_descriptor)];
static struct tss_entry kernel_tss;
static struct gdt_ptr   gdt_r;

static uint8_t initial_kernel_stack[16384] __attribute__((aligned(16)));
static uint8_t exception_ist_stack[16384]  __attribute__((aligned(16)));

static void set_gdt_gate(int num, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    struct gdt_entry *gdt = (struct gdt_entry *)gdt_bytes;
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

static void set_tss_gate(int num, uint64_t base, uint32_t limit) {
    struct tss_descriptor *tss_desc = (struct tss_descriptor *)&gdt_bytes[num * sizeof(struct gdt_entry)];
    tss_desc->length      = (limit & 0xFFFF);
    tss_desc->base_low    = (base & 0xFFFF);
    tss_desc->base_middle = (base >> 16) & 0xFF;
    tss_desc->flags1      = 0x89;
    tss_desc->flags2      = (limit >> 16) & 0x0F;
    tss_desc->base_high   = (base >> 24) & 0xFF;
    tss_desc->base_upper  = (base >> 32) & 0xFFFFFFFF;
    tss_desc->reserved    = 0;
}

void gdt_set_kernel_stack(uint64_t rsp) {
    kernel_tss.rsp0 = rsp;
}

void gdt_init(void) {
    kmemset(gdt_bytes, 0, sizeof(gdt_bytes));
    kmemset(&kernel_tss, 0, sizeof(kernel_tss));

    kernel_tss.rsp0 = (uint64_t)&initial_kernel_stack[sizeof(initial_kernel_stack)];
    kernel_tss.ist1 = (uint64_t)&exception_ist_stack[sizeof(exception_ist_stack)];

    set_gdt_gate(0, 0, 0, 0, 0);
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel CS (0x08)
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel DS (0x10)
    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User DS   (0x18 | 3 = 0x1B)
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User CS   (0x20 | 3 = 0x23)
    set_tss_gate(5, (uint64_t)&kernel_tss, sizeof(kernel_tss) - 1); // TSS (0x28)

    gdt_r.limit = sizeof(gdt_bytes) - 1;
    gdt_r.base  = (uint64_t)gdt_bytes;

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x28, %%ax\n"
        "ltr %%ax\n"
        : : "m"(gdt_r) : "rax"
    );

    kprintf("[GDT] 64-bit GDT and TSS loaded successfully.\n");
}
