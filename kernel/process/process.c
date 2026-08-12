#include "process.h"
#include "mm/memory.h"
#include "arch/x86_64/gdt.h"
#include "drivers/uart.h"
#include <string.h>

extern uint64_t kernel_rsp_temp;

static uint8_t kernel_stack_pool[16384] __attribute__((aligned(16)));

process_t *process_create(const elf_info_t *elf_info) {
    void *proc_mem = alloc_page();
    process_t *proc = (process_t *)proc_mem;
    proc->entry_point = elf_info->entry_point;
    proc->heap_curr   = (elf_info->max_vaddr + PAGE_SIZE - 1) & ~0xFFFULL;

    // Allocate 64KB for User Stack
    void *stack_phys = alloc_pages(USER_STACK_PAGES);
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, (uint64_t)stack_phys, USER_STACK_PAGES);

    // Setup initial stack for musl libc:
    uint8_t *phys_top = (uint8_t *)stack_phys + (USER_STACK_PAGES * PAGE_SIZE);
    char *arg_str = (char *)(phys_top - 64);
    strcpy(arg_str, "calc");

    uint64_t argv0_virt = USER_STACK_TOP - 64;

    uint64_t *sp = (uint64_t *)(phys_top - 256);

    *sp++ = 1;          // argc = 1
    *sp++ = argv0_virt; // argv[0]
    *sp++ = 0;          // argv[1] = NULL
    *sp++ = 0;          // envp[0] = NULL
    *sp++ = 0;          // AuxV AT_NULL type
    *sp++ = 0;          // AuxV AT_NULL val

    uint64_t stack_offset = 256;
    proc->user_rsp = (USER_STACK_TOP - stack_offset) & ~0xFULL;

    kprintf("[Process] Created user process: Entry=%p, Stack=%p, HeapBase=%p\n",
            proc->entry_point, proc->user_rsp, proc->heap_curr);

    return proc;
}

static void __attribute__((noinline, noreturn)) jump_to_user_code(uint64_t user_rsp, uint64_t user_rip) {
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        : : "r"(user_rsp), "r"(user_rip) : "memory"
    );
    __builtin_unreachable();
}

void process_jump_to_user(process_t *proc) {
    uint64_t kstack_top = (uint64_t)&kernel_stack_pool[sizeof(kernel_stack_pool)];
    gdt_set_kernel_stack(kstack_top);
    kernel_rsp_temp = kstack_top;

    uint64_t entry = proc->entry_point;
    uint64_t stack = proc->user_rsp;

    kprintf("[Process] Launching ELF process execution (RIP=%p, RSP=%p) ...\n", entry, stack);

    jump_to_user_code(stack, entry);
}
