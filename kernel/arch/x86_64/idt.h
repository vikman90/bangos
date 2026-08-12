#ifndef IDT_H
#ifndef KERNEL_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec_num, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} exception_frame_t;

void idt_init(void);
void exception_handler(exception_frame_t *frame);

#endif
#endif
