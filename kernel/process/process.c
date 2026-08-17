#include "process.h"
#include "mm/memory.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "drivers/uart.h"
#include "drivers/pit.h"
#include "fs/tarfs.h"
#include "lib/kstring.h"

extern uint64_t kernel_rsp_temp;
extern volatile uint64_t pit_ticks;

static process_t process_table[MAX_PROCESSES];
static int current_proc_idx = 0;
static int next_pid = 1;

void process_init(void) {
    kmemset(process_table, 0, sizeof(process_table));
    current_proc_idx = 0;
    next_pid = 1;
}

process_t *process_get_current(void) {
    return &process_table[current_proc_idx];
}

process_t *process_create_from_elf(const elf_info_t *elf_info, const char *name) {
    int idx = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return NULL;

    process_t *proc = &process_table[idx];
    kmemset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    proc->ppid = 0;
    proc->tgid = proc->pid;
    proc->is_thread = false;
    proc->active = true;
    proc->exited = false;
    proc->state = PROCESS_STATE_READY;
    proc->entry_point = elf_info->entry_point;
    proc->heap_curr   = (elf_info->max_vaddr + PAGE_SIZE - 1) & ~0xFFFULL;
    if (name) {
        kstrncpy(proc->name, name, sizeof(proc->name) - 1);
    }

    // Save segment info and create VMAs
    vmm_init_process(proc);
    proc->num_segments = elf_info->num_segments;
    for (size_t s = 0; s < elf_info->num_segments; s++) {
        proc->segments[s] = elf_info->segments[s];
        vma_create(proc, elf_info->segments[s].virt_addr,
                   elf_info->segments[s].virt_addr + (elf_info->segments[s].num_pages * PAGE_SIZE),
                   VMA_PROT_READ | VMA_PROT_WRITE | VMA_PROT_EXEC, VMA_MAP_PRIVATE);
    }

    // Allocate 16 KB Kernel Stack
    proc->kstack = (uint8_t *)alloc_pages(KERNEL_STACK_PAGES);
    proc->kstack_top = (uint64_t)proc->kstack + (KERNEL_STACK_PAGES * PAGE_SIZE);

    // Allocate 64 KB User Stack
    void *stack_phys = alloc_pages(USER_STACK_PAGES);
    proc->stack_phys_base = (uint64_t)stack_phys;
    proc->stack_num_pages = USER_STACK_PAGES;
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, (uint64_t)stack_phys, USER_STACK_PAGES);
    vma_create(proc, stack_virt_base, USER_STACK_TOP,
               VMA_PROT_READ | VMA_PROT_WRITE, VMA_MAP_PRIVATE | VMA_MAP_ANONYMOUS);

    // Setup initial stack for musl libc:

    uint8_t *phys_top = (uint8_t *)stack_phys + (USER_STACK_PAGES * PAGE_SIZE);
    char *arg_str = (char *)(phys_top - 64);
    kstrncpy(arg_str, name ? name : "app", 63);

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

    // Prepare context frame on process kernel stack for iretq
    context_frame_t *frame = (context_frame_t *)(proc->kstack_top - sizeof(context_frame_t));
    kmemset(frame, 0, sizeof(context_frame_t));
    frame->rip = proc->entry_point;
    frame->cs = 0x20 | 3;  // User CS
    frame->rflags = 0x202; // IF=1, reserved bit 1 set
    frame->rsp = proc->user_rsp;
    frame->ss = 0x18 | 3;  // User DS/SS
    proc->saved_frame = frame;

    // Initialize FPU/SSE state
    __asm__ volatile ("fninit; fxsave64 %0" : "=m"(proc->fpu_state));

    current_proc_idx = idx;

    kprintf("[Process] Initialized PID=%d (name='%s', Entry=%p, Stack=%p, HeapBase=%p)\n",
            proc->pid, proc->name, proc->entry_point, proc->user_rsp, proc->heap_curr);

    return proc;
}

void process_jump_to_user(process_t *proc) {
    gdt_set_kernel_stack(proc->kstack_top);
    kernel_rsp_temp = proc->kstack_top;
    proc->state = PROCESS_STATE_RUNNING;

    kprintf("[Process] Launching process PID=%d (RIP=%p, RSP=%p, RFLAGS=%p) ...\n",
            proc->pid, proc->entry_point, proc->user_rsp, proc->saved_frame->rflags);

    // Restore FPU state
    __asm__ volatile ("fxrstor64 %0" : : "m"(proc->fpu_state));

    switch_to_context_frame(proc->saved_frame);
}

int process_fork(context_frame_t *frame) {
    int child_idx = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            child_idx = i;
            break;
        }
    }

    if (child_idx == -1) {
        return -12; // -ENOMEM
    }

    process_t *parent = &process_table[current_proc_idx];
    process_t *child = &process_table[child_idx];
    kmemset(child, 0, sizeof(process_t));

    child->pid = next_pid++;
    child->ppid = parent->pid;
    child->tgid = child->pid;
    child->is_thread = false;
    child->active = true;
    child->exited = false;
    child->entry_point = parent->entry_point;
    child->heap_curr = parent->heap_curr;
    child->fs_base = parent->fs_base;
    kstrncpy(child->name, parent->name, sizeof(child->name) - 1);

    // Copy segment mappings from parent
    child->num_segments = parent->num_segments;
    for (size_t s = 0; s < parent->num_segments; s++) {
        child->segments[s] = parent->segments[s];
    }

    // Allocate dedicated 16 KB Kernel Stack for child
    child->kstack = (uint8_t *)alloc_pages(KERNEL_STACK_PAGES);
    child->kstack_top = (uint64_t)child->kstack + (KERNEL_STACK_PAGES * PAGE_SIZE);

    // Allocate dedicated 64 KB User Stack and copy parent's stack content
    void *child_stack_phys = alloc_pages(USER_STACK_PAGES);
    child->stack_phys_base = (uint64_t)child_stack_phys;
    child->stack_num_pages = USER_STACK_PAGES;
    kmemcpy(child_stack_phys, (const void *)parent->stack_phys_base, USER_STACK_PAGES * PAGE_SIZE);

    // Setup child context frame on its kernel stack
    context_frame_t *child_frame = (context_frame_t *)(child->kstack_top - sizeof(context_frame_t));
    kmemcpy(child_frame, frame, sizeof(context_frame_t));
    child_frame->rax = 0;          // Child return code = 0
    child_frame->rflags |= 0x202;  // Ensure IF=1
    child->saved_frame = child_frame;
    child->user_rsp = frame->rsp;

    // Copy parent FPU state
    kmemcpy(child->fpu_state, parent->fpu_state, sizeof(child->fpu_state));

    // Copy VMAs from parent
    child->mmap_curr_base = parent->mmap_curr_base;
    for (int v = 0; v < MAX_PROCESS_VMAS; v++) {
        child->vmas[v] = parent->vmas[v];
    }

    child->state = PROCESS_STATE_READY;


    kprintf("[Process] Fork created child process PID=%d (PPID=%d, KStack=%p)\n",
            child->pid, child->ppid, child->kstack_top);
    return child->pid;
}

int process_clone(unsigned long flags, void *child_stack, int *ptid, int *ctid, void *newtls, context_frame_t *frame) {
    (void)newtls;
    if (!(flags & 0x00000100)) { // If not CLONE_VM, standard process fork
        return process_fork(frame);
    }

    int thread_idx = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            thread_idx = i;
            break;
        }
    }

    if (thread_idx == -1) {
        return -12; // -ENOMEM
    }

    process_t *parent = &process_table[current_proc_idx];
    process_t *thread = &process_table[thread_idx];
    kmemset(thread, 0, sizeof(process_t));

    thread->pid = next_pid++;
    thread->ppid = parent->pid;
    thread->tgid = parent->tgid ? parent->tgid : parent->pid;
    thread->is_thread = true;
    thread->active = true;
    thread->exited = false;
    thread->entry_point = parent->entry_point;
    thread->heap_curr = parent->heap_curr;
    thread->fs_base = newtls ? (uint64_t)newtls : parent->fs_base;
    thread->stack_phys_base = parent->stack_phys_base;
    kstrncpy(thread->name, parent->name, sizeof(thread->name) - 1);

    // Share segment mappings with parent
    thread->num_segments = parent->num_segments;
    for (size_t s = 0; s < parent->num_segments; s++) {
        thread->segments[s] = parent->segments[s];
    }

    // Allocate dedicated 16 KB Kernel Stack for thread
    thread->kstack = (uint8_t *)alloc_pages(KERNEL_STACK_PAGES);
    thread->kstack_top = (uint64_t)thread->kstack + (KERNEL_STACK_PAGES * PAGE_SIZE);

    // Setup thread context frame
    context_frame_t *tframe = (context_frame_t *)(thread->kstack_top - sizeof(context_frame_t));
    kmemcpy(tframe, frame, sizeof(context_frame_t));
    if (child_stack) {
        tframe->rsp = (uint64_t)child_stack;
    }
    tframe->rax = 0;          // Clone returns 0 to thread
    tframe->rflags |= 0x202;  // Ensure IF=1
    thread->saved_frame = tframe;
    thread->user_rsp = tframe->rsp;

    if (ptid && (flags & 0x00100000)) { // CLONE_PARENT_SETTID
        *ptid = thread->pid;
    }
    if (ctid && (flags & 0x00200000)) { // CLONE_CHILD_CLEARTID
        thread->clear_child_tid = ctid;
    }

    // Copy FPU state
    kmemcpy(thread->fpu_state, parent->fpu_state, sizeof(thread->fpu_state));

    // Share/copy VMAs from parent
    thread->mmap_curr_base = parent->mmap_curr_base;
    for (int v = 0; v < MAX_PROCESS_VMAS; v++) {
        thread->vmas[v] = parent->vmas[v];
    }

    thread->state = PROCESS_STATE_READY;

    kprintf("[Thread] Created thread TID=%d (TGID=%d, Stack=%p)\n",
            thread->pid, thread->tgid, child_stack);
    return thread->pid;
}

int process_execve(const char *path, char *const argv[], char *const envp[]) {
    (void)envp;
    if (!path) return -14; // -EFAULT

    const void *elf_data = NULL;
    size_t elf_size = 0;

    if (tarfs_lookup(path, &elf_data, &elf_size) != 0) {
        kprintf("[Exec Error] Binary '%s' not found in ramdisk\n", path);
        return -2; // -ENOENT
    }

    elf_info_t elf_info;
    if (elf_load_binary(elf_data, elf_size, &elf_info) != 0) {
        kprintf("[Exec Error] Failed to parse ELF for '%s'\n", path);
        return -8; // -ENOEXEC
    }

    process_t *proc = process_get_current();
    proc->entry_point = elf_info.entry_point;
    proc->heap_curr   = (elf_info.max_vaddr + PAGE_SIZE - 1) & ~0xFFFULL;
    kstrncpy(proc->name, (argv && argv[0]) ? argv[0] : path, sizeof(proc->name) - 1);

    // Save segment info and create VMAs
    vmm_init_process(proc);
    proc->num_segments = elf_info.num_segments;
    for (size_t s = 0; s < elf_info.num_segments; s++) {
        proc->segments[s] = elf_info.segments[s];
        vma_create(proc, elf_info.segments[s].virt_addr,
                   elf_info.segments[s].virt_addr + (elf_info.segments[s].num_pages * PAGE_SIZE),
                   VMA_PROT_READ | VMA_PROT_WRITE | VMA_PROT_EXEC, VMA_MAP_PRIVATE);
    }

    // Setup fresh user stack
    if (!proc->stack_phys_base) {
        proc->stack_phys_base = (uint64_t)alloc_pages(USER_STACK_PAGES);
        proc->stack_num_pages = USER_STACK_PAGES;
    }
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, proc->stack_phys_base, USER_STACK_PAGES);
    vma_create(proc, stack_virt_base, USER_STACK_TOP,
               VMA_PROT_READ | VMA_PROT_WRITE, VMA_MAP_PRIVATE | VMA_MAP_ANONYMOUS);

    uint8_t *phys_top = (uint8_t *)proc->stack_phys_base + (USER_STACK_PAGES * PAGE_SIZE);
    // Setup argv0
    char *arg_str = (char *)(phys_top - 64);
    const char *src_name = (argv && argv[0]) ? argv[0] : path;
    kstrncpy(arg_str, src_name, 63);

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

    // Reset saved frame
    context_frame_t *frame = (context_frame_t *)(proc->kstack_top - sizeof(context_frame_t));
    kmemset(frame, 0, sizeof(context_frame_t));
    frame->rip = proc->entry_point;
    frame->cs = 0x20 | 3;
    frame->rflags = 0x202;
    frame->rsp = proc->user_rsp;
    frame->ss = 0x18 | 3;
    proc->saved_frame = frame;

    __asm__ volatile ("fninit; fxsave64 %0" : "=m"(proc->fpu_state));

    kprintf("[Process] Execve '%s' executing in PID=%d (Entry=%p, RSP=%p)\n",
            path, proc->pid, proc->entry_point, proc->user_rsp);

    mm_flush_tlb();
    process_jump_to_user(proc);
    return 0;
}

int process_wait4(int pid, int *status_ptr) {
    process_t *parent = process_get_current();
    while (1) {
        bool has_children = false;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (!process_table[i].active) continue;
            if (process_table[i].pid == pid || (pid <= 0 && process_table[i].ppid == parent->pid)) {
                has_children = true;
                if (process_table[i].state == PROCESS_STATE_ZOMBIE || process_table[i].exited) {
                    int child_pid = process_table[i].pid;
                    int code = process_table[i].exit_code;
                    if (status_ptr) {
                        *status_ptr = (code & 0xFF) << 8;
                    }
                    process_table[i].active = false;
                    process_table[i].exited = false;
                    process_table[i].state = PROCESS_STATE_FREE;
                    return child_pid;
                }
            }
        }
        if (!has_children) {
            return -10; // -ECHILD
        }
        __asm__ volatile ("sti; hlt");
    }
}

void process_exit(int code) {
    process_t *proc = process_get_current();

    if (proc->clear_child_tid) {
        *(proc->clear_child_tid) = 0;
        futex_wake((uint32_t *)proc->clear_child_tid, 1);
    }

    if (proc->pid == 1) {
        kprintf("\n[Process] Process PID=1 exited with status code: %d\n", code);
        kprintf("[Kernel] Init process (PID 1) terminated. System halting cleanly.\n");
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }

    if (proc->is_thread) {
        proc->state = PROCESS_STATE_ZOMBIE;
        proc->exit_code = code;
        proc->active = false;
        while (1) {
            __asm__ volatile ("sti; hlt");
        }
    }

    proc->exited = true;
    proc->exit_code = code;
    proc->state = PROCESS_STATE_ZOMBIE;

    // Wake parent if waiting
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].active && process_table[i].pid == proc->ppid) {
            if (process_table[i].state == PROCESS_STATE_SLEEPING) {
                process_table[i].state = PROCESS_STATE_READY;
            }
        }
    }

    while (1) {
        __asm__ volatile ("sti; hlt");
    }
}

int futex_wait(uint32_t *uaddr, uint32_t val) {
    if (!uaddr) return -14; // -EFAULT
    if (*uaddr != val) return -11; // -EAGAIN

    process_t *proc = process_get_current();
    proc->wait_channel = (uint64_t)uaddr;
    proc->state = PROCESS_STATE_SLEEPING;

    while (proc->wait_channel == (uint64_t)uaddr) {
        __asm__ volatile ("sti; hlt");
    }
    return 0;
}

int futex_wake(uint32_t *uaddr, int count) {
    if (!uaddr || count <= 0) return 0;
    int woken = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].active &&
            process_table[i].state == PROCESS_STATE_SLEEPING &&
            process_table[i].wait_channel == (uint64_t)uaddr) {
            process_table[i].wait_channel = 0;
            process_table[i].state = PROCESS_STATE_READY;
            woken++;
            if (woken >= count) break;
        }
    }
    return woken;
}

void schedule_yield(void) {
    __asm__ volatile ("sti; hlt");
}

context_frame_t *scheduler_tick(context_frame_t *current_frame) {
    pit_ticks++;

    // 1. Wake up sleeping processes whose timer expired
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].active && process_table[i].state == PROCESS_STATE_SLEEPING) {
            if (process_table[i].sleep_ticks_remaining > 0) {
                process_table[i].sleep_ticks_remaining--;
                if (process_table[i].sleep_ticks_remaining == 0 && process_table[i].wait_channel == 0) {
                    process_table[i].state = PROCESS_STATE_READY;
                }
            }
        }
    }

    // 2. Save current process frame
    process_t *curr = &process_table[current_proc_idx];
    if (curr->active) {
        curr->saved_frame = current_frame;
        if (curr->state == PROCESS_STATE_RUNNING) {
            curr->state = PROCESS_STATE_READY;
        }
    }

    // 3. Find next READY process in round-robin order
    int next_idx = -1;
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int candidate = (current_proc_idx + i) % MAX_PROCESSES;
        if (process_table[candidate].active && process_table[candidate].state == PROCESS_STATE_READY) {
            next_idx = candidate;
            break;
        }
    }

    if (next_idx == -1) {
        if (curr->active && (curr->state == PROCESS_STATE_READY || curr->state == PROCESS_STATE_RUNNING)) {
            next_idx = current_proc_idx;
        } else {
            next_idx = 0; // Fallback
        }
    }

    process_t *next = &process_table[next_idx];
    if (next_idx != current_proc_idx) {
        if (curr->active) {
            __asm__ volatile ("fxsave64 %0" : "=m"(curr->fpu_state));
        }
        __asm__ volatile ("fxrstor64 %0" : : "m"(next->fpu_state));

        if (next->fs_base) {
            write_msr(0xC0000100, next->fs_base);
        }

        // Remap code and stack segments if switching to different process (non-thread or different TGID)
        if (!next->is_thread || next->tgid != curr->tgid) {
            for (size_t s = 0; s < next->num_segments; s++) {
                map_user_pages(next->segments[s].virt_addr,
                               next->segments[s].phys_addr,
                               next->segments[s].num_pages);
            }
            if (next->stack_phys_base) {
                map_user_pages(USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE),
                               next->stack_phys_base, USER_STACK_PAGES);
            }
            mm_flush_tlb();
        }
        current_proc_idx = next_idx;
    }

    next->state = PROCESS_STATE_RUNNING;
    gdt_set_kernel_stack(next->kstack_top);
    kernel_rsp_temp = next->kstack_top;

    pic_send_eoi(0);
    return next->saved_frame;
}

context_frame_t *timer_interrupt_handler(context_frame_t *frame) {
    return scheduler_tick(frame);
}
