#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "loader/elf.h"

#define USER_STACK_TOP 0x7FFFF0000000ULL
#define USER_STACK_PAGES 16 // 64 KB user stack

#define MAX_PROCESSES 16

typedef struct {
    int      pid;
    int      ppid;
    uint64_t entry_point;
    uint64_t user_rsp;
    uint64_t heap_curr;
    bool     active;
    bool     exited;
    int      exit_code;
    uint64_t saved_rsp;
    uint64_t saved_rip;
    uint64_t saved_rflags;
} process_t;

void       process_init(void);
process_t *process_create_from_elf(const elf_info_t *elf_info, const char *name);
process_t *process_get_current(void);
int        process_spawn_elf(const void *elf_data, size_t elf_size, const char *argv0);
int        process_execve(const char *path, char *const argv[], char *const envp[]);
int        process_fork(uint64_t user_rsp, uint64_t user_rip, uint64_t user_rflags);
int        process_wait4(int pid, int *status_ptr);
void       process_exit(int code);
void       process_jump_to_user(process_t *proc);

#endif /* PROCESS_H */
