#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "loader/elf.h"
#include "arch/x86_64/idt.h"
#include "mm/vmm.h"

#include "fs/vfs.h"

#define USER_STACK_TOP 0x7FFFF0000000ULL
#define USER_STACK_PAGES 16 // 64 KB user stack
#define KERNEL_STACK_PAGES 4 // 16 KB kernel stack
#define MAX_PROCESSES 32

typedef enum {
    PROCESS_STATE_FREE = 0,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_ZOMBIE
} process_state_t;

typedef struct process {
    // 512-byte FPU / SSE state (must be at offset 0 and 64-byte aligned for fxsave64/fxrstor64)
    uint8_t         fpu_state[512] __attribute__((aligned(64)));

    int             pid;
    int             ppid;
    int             tgid;
    bool            is_thread;
    bool            active;
    bool            exited;
    process_state_t state;
    int             exit_code;

    uint64_t        entry_point;
    uint64_t        user_rsp;
    uint64_t        heap_curr;
    uint64_t        fs_base;
    uint64_t        mmap_curr_base;

    // Memory tracking
    size_t          num_segments;
    elf_segment_t   segments[MAX_ELF_SEGMENTS];
    uint64_t        stack_phys_base;
    size_t          stack_num_pages;
    vm_area_t       vmas[MAX_PROCESS_VMAS];

    // File descriptors
    file_desc_t     fd_table[VFS_MAX_FD];

    // Dedicated kernel stack
    uint8_t        *kstack;
    uint64_t        kstack_top;
    context_frame_t *saved_frame;

    // Sleep & Synchronization
    uint64_t        sleep_ticks_remaining;
    uint64_t        wait_channel;
    int            *clear_child_tid;

    char            name[32];
} __attribute__((aligned(64))) process_t;

void             process_init(void);
process_t       *process_create_from_elf(const elf_info_t *elf_info, const char *name);
process_t       *process_get_current(void);
int              process_spawn_elf(const void *elf_data, size_t elf_size, const char *argv0);
int              process_execve(const char *path, char *const argv[], char *const envp[]);
int              process_fork(context_frame_t *frame);
int              process_clone(unsigned long flags, void *child_stack, int *ptid, int *ctid, void *newtls, context_frame_t *frame);
int              process_wait4(int pid, int *status_ptr);
void             process_exit(int code);
void             process_jump_to_user(process_t *proc);
int              futex_wait(uint32_t *uaddr, uint32_t val);
int              futex_wake(uint32_t *uaddr, int count);
void             schedule_yield(void);
context_frame_t *scheduler_tick(context_frame_t *current_frame);

#endif /* PROCESS_H */
