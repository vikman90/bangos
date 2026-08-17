# 05 - Process Management, Preemptive Scheduling & Synchronization

BangOS implements a complete **multi-process and multithreading runtime** featuring:
1. Process Control Block (`process_t`) architecture.
2. Binary execution via `fork()`, `clone()`, `execve()`, and `wait4()`.
3. musl C user stack initialization (`argc`, `argv`, `envp`, Auxiliary Vectors).
4. Preemptive 100 Hz Round-Robin scheduling driven by the 8254 PIT timer.
5. In-kernel **Fast Userspace Mutex (Futex)** synchronization engine.

---

## 🗂️ Process Control Block (PCB) (`kernel/process/process.h`)

Every task (process or thread) is represented by a `process_t` structure in the kernel process table (`process_table[MAX_PROCESSES]`):

```c
typedef struct process {
    int              pid;                  // Unique Process ID
    int              ppid;                 // Parent Process ID
    int              tgid;                 // Thread Group ID
    bool             is_thread;            // True if sharing address space (CLONE_VM)
    bool             active;               // Slot allocated
    bool             exited;               // Process terminated
    int              exit_code;            // Termination status code
    process_state_t  state;                // FREE, READY, RUNNING, SLEEPING, ZOMBIE
    char             name[64];             // Process display name

    uint64_t         entry_point;          // ELF entry point (RIP)
    uint64_t         user_rsp;             // User stack pointer (RSP)
    uint64_t         heap_curr;            // Current break boundary (brk)
    uint64_t         fs_base;              // Thread Local Storage (MSR_FS_BASE)

    uint8_t         *kstack;               // Dedicated 16 KB Kernel Stack
    uint64_t         kstack_top;           // Top of Kernel Stack (RSP0)

    uint64_t         stack_phys_base;      // Physical address of User Stack (64 KB)
    size_t           stack_num_pages;      // Number of user stack pages (16)

    context_frame_t *saved_frame;          // Pointer to saved CPU registers
    uint8_t          fpu_state[512] __attribute__((aligned(16))); // FXSAVE buffer

    vm_area_t        vmas[MAX_PROCESS_VMAS]; // Virtual Memory Areas
    uint64_t         mmap_curr_base;       // Base address for dynamic mmap allocations

    elf_segment_t    segments[MAX_ELF_SEGMENTS]; // PT_LOAD segment mappings
    size_t           num_segments;

    uint64_t         wait_channel;         // Futex wait address (0 if not waiting)
    uint64_t         sleep_ticks_remaining;// Nanosleep timer ticks remaining
    int             *clear_child_tid;      // CLONE_CHILD_CLEARTID address
} process_t;
```

---

## 🏗️ Userland Stack Layout for musl libc

When `process_create_from_elf()` initializes a new process, it prepares the initial user stack (`USER_STACK_TOP = 0x00007FFFFFFFF000`) to conform to the **System V AMD64 ABI** expectations of the static `musl` C runtime:

```text
Higher Addresses (0x00007FFFFFFFF000)
+------------------------------------+
| Argument String (e.g. "init\0")    | <- USER_STACK_TOP - 64
+------------------------------------+
| ... (16-byte alignment padding)    |
+------------------------------------+
| AuxV[0].a_val = 0 (AT_NULL)        |
| AuxV[0].a_type = 0 (AT_NULL)       | <- End of Auxiliary Vector
| envp[0] = NULL                     | <- End of Environment pointers
| argv[1] = NULL                     | <- End of Argument pointers
| argv[0] = Pointer to arg string    | <- Points to "init\0" above
| argc = 1                           | <- Initial stack pointer (proc->user_rsp)
+------------------------------------+
Lower Addresses (Grows downwards)
```

---

## 👥 Process Cloning (`fork` vs `clone`)

### 1. `process_fork()` (Independent Process Creation)
`process_fork()` duplicates the parent process:
1. Allocates a new PID and a dedicated 16 KB kernel stack.
2. Allocates a fresh 64 KB user stack and copies the entire parent stack contents (`kmemcpy`).
3. Duplicates all Virtual Memory Areas (VMAs) and segment descriptors.
4. Duplicates the parent's FPU/SSE state (`fpu_state`).
5. Sets `child_frame->rax = 0` (child returns 0) and returns `child->pid` to the parent.

### 2. `process_clone()` (Lightweight Thread Creation)
When called with `CLONE_VM`, `process_clone()` creates a thread:
1. Shares code and data segment mappings with the parent without duplicating memory.
2. Assigns a caller-provided user stack pointer (`child_stack`).
3. Sets `thread->tgid = parent->tgid` (same thread group).
4. Handles `CLONE_CHILD_CLEARTID` (for automated futex notification upon thread exit).

---

## ⏱️ Preemptive Multitasking & Context Switching (`scheduler_tick`)

The 8254 Programmable Interval Timer (PIT) generates hardware interrupts on **IRQ 0 (Vector 32)** at **100 Hz (every 10 ms)**.

```text
PIT Hardware Timer fires IRQ 0 (Vector 32)
                       |
                       v
+--------------------------------------------------------------+
| isr_32 (kernel/arch/x86_64/isr.s)                            |
| - Pushes general purpose registers to build context_frame_t  |
| - Calls timer_interrupt_handler(frame)                       |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| scheduler_tick(current_frame) (kernel/process/process.c)     |
| 1. Decrement sleep_ticks_remaining for sleeping tasks;       |
|    wake tasks whose timer expired (state -> READY).          |
| 2. Save current_frame pointer in curr->saved_frame.          |
| 3. If switching process: save FPU state (fxsave64).          |
| 4. Select next READY task via Round-Robin scan.              |
| 5. Restore next task's FPU state (fxrstor64).                |
| 6. If non-thread switch: remap memory segments & flush TLB.  |
| 7. Set MSR_FS_BASE to next->fs_base (TLS pointer).           |
| 8. Update TSS.RSP0 and kernel_rsp_temp to next->kstack_top.  |
| 9. Send EOI to 8259 PIC (pic_send_eoi(0)).                   |
| 10. Return next->saved_frame pointer.                        |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| isr_32 restores registers from next->saved_frame and         |
| executes iretq -> CPU resumes execution in new task!         |
+--------------------------------------------------------------+
```

---

## 🔒 In-Kernel Futex Synchronization (`SYS_FUTEX`)

BangOS provides fast user-space locking primitives via `futex_wait()` and `futex_wake()`:

```c
int futex_wait(uint32_t *uaddr, uint32_t val) {
    if (!uaddr) return -14; // -EFAULT
    if (*uaddr != val) return -11; // -EAGAIN (value changed before sleeping)

    process_t *proc = process_get_current();
    proc->wait_channel = (uint64_t)uaddr;
    proc->state = PROCESS_STATE_SLEEPING;

    // Yield CPU while waiting for futex_wake
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
```

This mechanism allows userland code in [`userland/include/synch.h`](file:///root/test/little-bang/userland/include/synch.h) to build efficient **Atomic CAS Mutexes** and **Counting Semaphores** that spin briefly in user space and sleep in the kernel only when contention occurs.
