# 09 - Userland Runtime, Applications & Concurrency

BangOS executes standard userland programs compiled statically with **`musl-gcc`**. The userland runtime runs exclusively in **Ring 3 (CPL 3)** and communicates with the microkernel via the Linux x86_64 system call interface.

---

## 🛠️ Static musl Compilation & `initrd.tar` Packaging

All userland source files reside in [`userland/src/`](file:///root/test/little-bang/userland/src/) and compile into standalone 64-bit ELF executables inside `userland/bin/`:

```makefile
CC = musl-gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -static -lm

$(BIN_DIR)/calc: src/calc.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

initrd.tar: $(TARGETS)
	tar -cf initrd.tar --format=ustar -C bin init calc sysinfo bench tasks threads \
	    test_syscall_safety test_vmm_demand test_process_lifecycle
```

---

## 👑 Supervisor Process: `/bin/init` (PID 1) (`userland/src/init.c`)

When the kernel finishes early initialization, it loads and jumps into `/bin/init` as the root supervisor process:

1. **System Banner**: Invokes `uname()` and `sysinfo()` to display kernel version, memory metrics, and uptime.
2. **Interactive Launcher**: Provides a menu to launch standalone binaries via `fork()` + `execve()` + `waitpid()`:

```c
static int launch_program(const char *path, const char *name) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child: execute binary
        char *argv[] = { (char *)name, NULL };
        execve(path, argv, NULL);
        exit(127);
    } else if (pid > 0) {
        // Parent: wait for child process termination
        int status = 0;
        waitpid(pid, &status, 0);
        return status;
    }
    return -1;
}
```

---

## 📱 Standalone Application Catalog

```text
======================================================================
        BangOS (x86_64) - Bare Metal Kernel v0.3.0
     PID: 1 (init) | RAM: 128 MB Total (126 MB Free) | Uptime: 0 s
======================================================================

Available Standalone Applications (Multi-ELF Ramdisk):

  [1] Geometric Calculator           (execve /bin/calc)
  [2] System Information & Uname     (execve /bin/sysinfo)
  [3] CPU FPU/SSE & Timer Benchmark  (execve /bin/bench)
  [4] Preemptive Multitasking Tasks  (execve /bin/tasks)
  [5] Multithreading & Mutex Sync    (execve /bin/threads)
  [6] Disk Explorer & Storage Mgr    (execve /bin/disktool)
  [7] Run Specification Test Suites  (execve /bin/test_*)
  [8] Network Fetch & HTTP Client    (execve /bin/netfetch)
  [9] Shutdown / Halt System         (exit)

Select an option [1-9]:
```

### 1. `/bin/calc` (Geometric Calculator)
Demonstrates formatted console I/O (`scanf("%lf")`, `printf("%.2f")`) and hardware FPU math execution (`sqrt()`).

### 2. `/bin/sysinfo` (Hardware & OS Diagnostics)
Queries and displays system metrics via `uname()` and `sysinfo()` system calls (RAM total/used/free, uptime, PID, host architecture).

### 3. `/bin/bench` (Subsystem Benchmark Suite)
Evaluates CPU and kernel memory performance across 4 automated benchmarks:

- **FPU/SSE Math**: Calculates Leibniz Pi over 2,000,000 iterations measuring Mops/s throughput.
- **Dynamic Heap Memory**: Executes 1,000 `malloc()` / `free()` cycles over `brk()` and `mmap()`.
- **Demand Paging & VMM**: Allocates 4 MB anonymous memory via `mmap()`, touches 1,024 4KB pages to trigger on-demand `#PF` faults, verifies memory integrity, tests `mprotect()`, and unmaps via `munmap()`.
- **Nanosleep Precision**: Validates nanosecond-resolution sleep timing via `clock_gettime()`.

### 4. `/bin/tasks` (Preemptive Multitasking Demo)
Demonstrates preemptive Round-Robin multitasking by spawning background computation workers (prime number calculations) while concurrently maintaining an interactive UART shell.

### 5. `/bin/threads` (Multithreading & Synchronization Suite)
Demonstrates kernel thread creation (`CLONE_VM`) and synchronization primitives:

- **Race Condition Demo**: Unsynchronized concurrent increments exhibiting data races from preemptive timer interrupts.
- **Mutex Synchronization**: Atomic CAS + Futex mutex protecting shared data structures with 100% consistency.
- **Producer-Consumer Queue Pipeline**: Bounded buffer managed with Counting Semaphores and Mutexes.

### 6. `/bin/disktool` (Storage Explorer & ext2 Inspector)
Provides an interactive TUI for inspecting ATA storage drives, viewing ext2 superblocks, directory hierarchies, reading files, and running persistent write tests.

### 7. `/bin/netfetch` (Network Diagnostics & HTTP/1.1 Web Client)
Interactive networking suite with options to:

- Probe VirtIO-Net MAC address, IP settings, and gateway configuration.
- Send ICMP Echo ping requests to the default gateway (`10.0.2.2`).
- Perform RFC 1035 UDP DNS hostname queries against the nameserver (`10.0.2.3:53`).
- Connect to remote HTTP web servers over TCP stream sockets and fetch HTML response payloads.
- Execute automated end-to-end network verification test suites.

---

## 🧵 Concurrency Engine (`userland/include/synch.h`)

BangOS userland provides lightweight synchronization primitives built on atomic operations and the `SYS_FUTEX` system call:

### 1. Fast Atomic CAS Mutex (`mutex_t`)
```c
typedef struct { volatile int val; } mutex_t;

static inline void mutex_lock(mutex_t *m) {
    int c = 0;
    // Fast path: Try atomic Compare-And-Swap (CAS) in user space
    if (__atomic_compare_exchange_n(&m->val, &c, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2) {
        c = __atomic_exchange_n(&m->val, 2, __ATOMIC_ACQUIRE);
    }
    // Slow path: Sleep in kernel via futex if contended
    while (c != 0) {
        syscall(SYS_FUTEX, &m->val, FUTEX_WAIT, 2, NULL, NULL, 0);
        c = __atomic_exchange_n(&m->val, 2, __ATOMIC_ACQUIRE);
    }
}

static inline void mutex_unlock(mutex_t *m) {
    if (__atomic_fetch_sub(&m->val, 1, __ATOMIC_RELEASE) != 1) {
        m->val = 0;
        // Wake 1 sleeping waiter in kernel
        syscall(SYS_FUTEX, &m->val, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}
```

### 2. Thread Handle & Lifetime (`thread_create_handle`)
Threads are created using `SYS_CLONE` with `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD`. Upon thread exit, the kernel clears the thread TID and wakes waiters via `futex_wake`.
