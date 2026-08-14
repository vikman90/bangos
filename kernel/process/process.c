#include "process.h"
#include "mm/memory.h"
#include "arch/x86_64/gdt.h"
#include "drivers/uart.h"
#include "fs/tarfs.h"
#include "lib/kstring.h"

extern uint64_t kernel_rsp_temp;

static uint8_t kernel_stack_pool[16384] __attribute__((aligned(16)));

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
    int idx = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            idx = i;
            break;
        }
    }

    process_t *proc = &process_table[idx];
    kmemset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    proc->active = true;
    proc->entry_point = elf_info->entry_point;
    proc->heap_curr   = (elf_info->max_vaddr + PAGE_SIZE - 1) & ~0xFFFULL;

    // Allocate 64KB for User Stack
    void *stack_phys = alloc_pages(USER_STACK_PAGES);
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, (uint64_t)stack_phys, USER_STACK_PAGES);

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

    current_proc_idx = idx;

    kprintf("[Process] Initialized PID=%d (name='%s', Entry=%p, Stack=%p, HeapBase=%p)\n",
            proc->pid, name ? name : "app", proc->entry_point, proc->user_rsp, proc->heap_curr);

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

    kprintf("[Process] Launching process PID=%d (RIP=%p, RSP=%p) ...\n", proc->pid, entry, stack);

    jump_to_user_code(stack, entry);
}

int process_fork(uint64_t user_rsp, uint64_t user_rip, uint64_t user_rflags) {
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
    child->active = true;
    child->exited = false;
    child->entry_point = parent->entry_point;
    child->user_rsp = user_rsp;
    child->heap_curr = parent->heap_curr;
    child->saved_rip = user_rip;
    child->saved_rsp = user_rsp;
    child->saved_rflags = user_rflags;

    // Switch current process index to the newly created child!
    current_proc_idx = child_idx;

    kprintf("[Process] Fork created child process PID=%d (PPID=%d)\n", child->pid, child->ppid);
    return child->pid;
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

    // Allocate new user stack
    void *stack_phys = alloc_pages(USER_STACK_PAGES);
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, (uint64_t)stack_phys, USER_STACK_PAGES);

    uint8_t *phys_top = (uint8_t *)stack_phys + (USER_STACK_PAGES * PAGE_SIZE);

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

    kprintf("[Process] Execve '%s' executing in PID=%d (Entry=%p, RSP=%p)\n",
            path, proc->pid, proc->entry_point, proc->user_rsp);

    process_jump_to_user(proc);
    return 0;
}

int process_wait4(int pid, int *status_ptr) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid || (pid <= 0 && process_table[i].ppid == process_table[current_proc_idx].pid)) {
            if (process_table[i].exited) {
                int code = process_table[i].exit_code;
                int child_pid = process_table[i].pid;
                if (status_ptr) {
                    *status_ptr = (code & 0xFF) << 8;
                }
                process_table[i].active = false;
                process_table[i].exited = false;
                return child_pid;
            }
        }
    }
    return 0;
}

void process_exit(int code) {
    process_t *proc = process_get_current();
    kprintf("\n[Process] Process PID=%d exited with status code: %d\n", proc->pid, code);

    if (proc->pid == 1) {
        kprintf("[Kernel] Init process (PID 1) terminated. System halting cleanly.\n");
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }

    // Child process exited: mark terminated and return to parent (PID 1)
    proc->exited = true;
    proc->exit_code = code;
    proc->active = false;

    // Switch back to init (PID 1)
    current_proc_idx = 0;
    process_t *parent = &process_table[0];
    kprintf("[Process] Switching context back to Parent PID=%d\n", parent->pid);

    const void *init_elf_data = NULL;
    size_t init_elf_size = 0;
    if (tarfs_lookup("/bin/init", &init_elf_data, &init_elf_size) != 0) {
        tarfs_lookup("init", &init_elf_data, &init_elf_size);
    }

    elf_info_t elf_info;
    elf_load_binary(init_elf_data, init_elf_size, &elf_info);
    parent->entry_point = elf_info.entry_point;
    parent->heap_curr = (elf_info.max_vaddr + PAGE_SIZE - 1) & ~0xFFFULL;

    void *stack_phys = alloc_pages(USER_STACK_PAGES);
    uint64_t stack_virt_base = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
    map_user_pages(stack_virt_base, (uint64_t)stack_phys, USER_STACK_PAGES);

    uint8_t *phys_top = (uint8_t *)stack_phys + (USER_STACK_PAGES * PAGE_SIZE);
    char *arg_str = (char *)(phys_top - 64);
    kstrncpy(arg_str, "init", 63);

    uint64_t argv0_virt = USER_STACK_TOP - 64;
    uint64_t *sp = (uint64_t *)(phys_top - 256);

    *sp++ = 1;
    *sp++ = argv0_virt;
    *sp++ = 0;
    *sp++ = 0;
    *sp++ = 0;
    *sp++ = 0;

    parent->user_rsp = (USER_STACK_TOP - 256) & ~0xFULL;
    process_jump_to_user(parent);
}
