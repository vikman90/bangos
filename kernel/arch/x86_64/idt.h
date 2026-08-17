#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec_num, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} context_frame_t;

typedef context_frame_t exception_frame_t;

void idt_init(void);
void exception_handler(exception_frame_t *frame);
context_frame_t *timer_interrupt_handler(context_frame_t *frame);
void switch_to_context_frame(context_frame_t *frame) __attribute__((noreturn));

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

#endif /* IDT_H */

