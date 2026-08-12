#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "loader/elf.h"

#define USER_STACK_TOP 0x7FFFF0000000ULL
#define USER_STACK_PAGES 16 // 64 KB user stack

typedef struct {
    uint64_t entry_point;
    uint64_t user_rsp;
    uint64_t heap_curr;
} process_t;

process_t *process_create(const elf_info_t *elf_info);
void       process_jump_to_user(process_t *proc);

#endif /* PROCESS_H */
