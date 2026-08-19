# 04 - System Call Subsystem (Linux x86_64 ABI)

BangOS implements the native **Linux x86_64 System Call Interface** using the hardware `syscall` / `sysret` instruction mechanism. This enables unmodified binaries compiled statically with `musl-gcc` to execute directly on the bare-metal kernel.

---

## ⚡ Hardware MSR Configuration (`kernel/syscall/syscall.c`)

In Long Mode, system calls do not use slow software interrupt gates (`int 0x80`). Instead, the CPU executes the native `syscall` instruction, which switches from Ring 3 to Ring 0 in a few clock cycles by reading four Model-Specific Registers (MSRs):

| MSR Address | MSR Constant | Configured Value | Purpose |
| :--- | :--- | :--- | :--- |
| `0xC0000080` | `IA32_EFER` | `EFER.SCE = 1` (Bit 0) | System Call Enable (activates `syscall`/`sysret` instructions). |
| `0xC0000081` | `MSR_STAR` | `0x0010000800000000` | Defines target GDT segment selectors for Kernel (`0x08`/`0x10`) and User (`0x18`/`0x20`). |
| `0xC0000082` | `MSR_LSTAR` | `&syscall_entry` | Ring 0 target `RIP` address jumped to when `syscall` is executed. |
| `0xC0000084` | `MSR_SFMASK`| `0x200` (Bit 9) | RFLAGS mask: automatically clears the Interrupt Flag (`IF`) on syscall entry. |

```c
void syscall_init_msrs(void) {
    uint64_t efer = read_msr(0xC0000080);
    efer |= 1ULL; // Enable SCE (System Call Extension)
    write_msr(0xC0000080, efer);

    // STAR MSR: Bits 32-47 = Kernel CS (0x08), Bits 48-63 = User CS/SS Base (0x10)
    uint64_t star = ((uint64_t)0x0010 << 48) | ((uint64_t)0x0008 << 32);
    write_msr(0xC0000081, star);

    // LSTAR MSR: Target 64-bit RIP address for syscall_entry
    write_msr(0xC0000082, (uint64_t)&syscall_entry);

    // SFMASK MSR: Mask IF (Interrupt Flag 0x200) to disable interrupts upon entry
    write_msr(0xC0000084, 0x200);
}
```

---

## 📋 System V AMD64 ABI Register Passing Convention

When a userland binary invokes `syscall`:

* **Syscall Number**: Loaded into register `RAX`.
* **Arguments (Up to 6)**:
  1. `RDI` (Argument 1)
  2. `RSI` (Argument 2)
  3. `RDX` (Argument 3)
  4. `R10` (Argument 4 — note that `R10` is used instead of `RCX` because `syscall` clobbers `RCX`)
  5. `R8`  (Argument 5)
  6. `R9`  (Argument 6)
* **Hardware Saved State**:
  - `RCX`: Saves userland `RIP`.
  - `R11`: Saves userland `RFLAGS`.
* **Return Code**: Evaluated in `RAX`. Positive integers or `0` denote success; negative numbers denote standard Linux `-ERRNO` codes (e.g. `-EINVAL = -22`).

---

## ⚙️ Assembly Trampoline (`kernel/arch/x86_64/syscall_entry.s`)

Because userland stack pointers (`RSP`) cannot be trusted by the kernel, `syscall_entry` immediately swaps to the active process's kernel stack before invoking C handlers:

```assembly
.global syscall_entry
syscall_entry:
    // 1. Temporarily stash user RSP and switch to process kernel stack
    movq %rsp, user_rsp_temp(%rip)
    movq kernel_rsp_temp(%rip), %rsp

    // 2. Construct context_frame_t on kernel stack
    pushq $0x1B                   // User SS (0x18 | 3)
    pushq user_rsp_temp(%rip)     // User RSP
    pushq %r11                    // User RFLAGS (saved by hardware)
    pushq $0x23                   // User CS (0x20 | 3)
    pushq %rcx                    // User RIP (saved by hardware)
    pushq $0                      // Error Code
    pushq $0                      // Vector Number

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // 3. Align stack to 16-byte boundary for C calling convention
    movq %rsp, %r9                // 6th arg: context_frame_t*
    subq $8, %rsp

    // 4. Pass syscall arguments to do_syscall(sys_num, arg1, arg2, arg3, arg4, frame)
    movq %rax, %rdi               // Arg 1: Syscall number
    movq %rsi, %rdx               // Arg 3: 2nd syscall arg
    movq %r10, %r8                // Arg 5: 4th syscall arg
    movq 120(%rsp), %rsi          // Arg 2: 1st syscall arg (RDI saved in frame)
    movq 104(%rsp), %rcx          // Arg 4: 3rd syscall arg (RDX saved in frame)

    call do_syscall

    addq $8, %rsp
    movq %rax, 112(%rsp)          // Store return value in frame->rax

    // 5. Restore registers and return to userland via sysretq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp                // Pop vector and error code
    popq %rcx                     // Restore user RIP into RCX
    addq $8, %rsp                 // Skip CS
    popq %r11                     // Restore user RFLAGS into R11
    popq %rsp                     // Restore user RSP

    sysretq                       // Return to Ring 3 (loads RIP from RCX, RFLAGS from R11)
```

---

## 🛠️ Implemented System Calls Reference (`kernel/syscall/syscall.c`)

| Syscall # | Constant | Arguments | Return / Behavior |
| :---: | :--- | :--- | :--- |
| **`0`** | `SYS_READ` | `int fd, char *buf, size_t count` | Reads from UART (fd 0) or VFS file descriptors (fd >= 3). Returns bytes read, `-EBADF` (-9), or `-EFAULT` (-14). |
| **`1`** | `SYS_WRITE` | `int fd, const char *buf, size_t count` | Writes to UART (fd 1,2) or VFS files (fd >= 3). Returns bytes written. |
| **`2`** | `SYS_OPEN` | `const char *path, int flags, int mode` | Opens or creates file node via VFS; allocates new file descriptor (fd >= 3). |
| **`3`** | `SYS_CLOSE` | `int fd` | Closes open file descriptor and releases VFS node reference. |
| **`4`** | `SYS_STAT` | `const char *path, struct stat *statbuf` | Populates file size, permissions, and mode for given pathname. |
| **`5`** | `SYS_FSTAT` | `int fd, struct stat *statbuf` | Populates file metadata for open file descriptor. |
| **`7`** | `SYS_POLL` | `struct pollfd *fds, nfds_t nfds, int timeout` | Non-blocking poll; returns `1` (I/O ready). |
| **`8`** | `SYS_LSEEK` | `int fd, off_t offset, int whence` | Repositions file read/write offset (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`). |
| **`9`** | `SYS_MMAP` | `void *addr, size_t len, int prot, int flags, int fd, off_t off` | Allocates a Virtual Memory Area (VMA) and memory pages dynamically. |
| **`10`** | `SYS_MPROTECT` | `void *addr, size_t len, int prot` | Modifies VMA permissions and updates hardware page table entry flags. |
| **`11`** | `SYS_MUNMAP` | `void *addr, size_t len` | Releases virtual memory range, reclaims physical frames, and trims VMAs. |
| **`12`** | `SYS_BRK` | `uint64_t brk_addr` | Queries current heap pointer (`brk(0)`) or expands process heap dynamically. |
| **`13`** | `SYS_RT_SIGACTION` | `int sig, const struct sigaction *act, ...` | Signal handler registration stub (returns `0`). |
| **`14`** | `SYS_RT_SIGPROCMASK`| `int how, const sigset_t *set, ...` | Signal mask configuration stub (returns `0`). |
| **`16`** | `SYS_IOCTL` | `int fd, unsigned long cmd, void *arg` | Handles terminal attributes (e.g. `TIOCGWINSZ` returning 80x24 window size). |
| **`20`** | `SYS_WRITEV` | `int fd, const struct iovec *iov, int iovcnt` | Vectorized scatter-gather output used by `printf()` / `vfprintf()`. |
| **`24`** | `SYS_SCHED_YIELD` | *none* | Cooperatively yields remaining CPU quantum to next ready task. |
| **`35`** | `SYS_NANOSLEEP` | `const struct timespec *req, struct timespec *rem` | High-precision sleep using calibrated timestamp counter (TSC) and PIT timer ticks. |
| **`39`** | `SYS_GETPID` | *none* | Returns current process group / process ID (`proc->tgid`). |
| **`41`** | `SYS_SOCKET` | `int domain, int type, int protocol` | Allocates network socket endpoint (`AF_INET`, `SOCK_STREAM` / `SOCK_DGRAM`) and wraps in VFS file descriptor. |
| **`42`** | `SYS_CONNECT` | `int sockfd, const struct sockaddr *addr, socklen_t addrlen` | Initiates TCP 3-way handshake (`SYN`/`SYN+ACK`/`ACK`) to remote host. |
| **`44`** | `SYS_SENDTO` | `int fd, const void *buf, size_t len, int flags, ...` | Transmits network stream or datagram payload via TCP/IP or UDP. |
| **`45`** | `SYS_RECVFROM`| `int fd, void *buf, size_t len, int flags, ...` | Receives incoming payload from socket ring buffer. |
| **`46`** | `SYS_SENDMSG` | `int fd, const struct msghdr *msg, int flags` | Message send stub for POSIX network compatibility. |
| **`47`** | `SYS_RECVMSG` | `int fd, struct msghdr *msg, int flags` | Message receive stub for POSIX network compatibility. |
| **`48`** | `SYS_SHUTDOWN`| `int sockfd, int how` | Shuts down reading, writing, or both on a connected socket. |
| **`49`** | `SYS_BIND` | `int sockfd, const struct sockaddr *addr, socklen_t addrlen` | Binds local network address. |
| **`50`** | `SYS_LISTEN` | `int sockfd, int backlog` | Configures socket for listening. |
| **`51`** | `SYS_GETSOCKNAME`| `int sockfd, struct sockaddr *addr, socklen_t *addrlen` | Returns local socket endpoint information. |
| **`52`** | `SYS_GETPEERNAME`| `int sockfd, struct sockaddr *addr, socklen_t *addrlen` | Returns remote peer address information. |
| **`54`** | `SYS_SETSOCKOPT`| `int sockfd, int level, int optname, const void *optval, socklen_t optlen` | Sets socket options. |
| **`55`** | `SYS_GETSOCKOPT`| `int sockfd, int level, int optname, void *optval, socklen_t *optlen` | Queries socket options. |
| **`56`** | `SYS_CLONE` | `unsigned long flags, void *child_stack, int *ptid, int *ctid, void *newtls` | Creates execution context: processes or threads (`CLONE_VM`). |
| **`57`** | `SYS_FORK` | *none* | Creates child process with duplicated memory mappings and execution state. |
| **`58`** | `SYS_VFORK` | *none* | Creates child process context. |
| **`59`** | `SYS_EXECVE` | `const char *path, char *const argv[], char *const envp[]` | Replaces current process with new standalone ELF from TarFS ramdisk. |
| **`60`** | `SYS_EXIT` | `int status` | Terminates process/thread, notifies parent via `wait4`, or halts system if PID 1. |
| **`61`** | `SYS_WAIT4` | `pid_t pid, int *status, int options, struct rusage *ru` | Waits for child process termination and retrieves exit status code. |
| **`63`** | `SYS_UNAME` | `struct utsname *buf` | Populates system name ("BangOS"), release ("0.3.0"), architecture ("x86_64"). |
| **`99`** | `SYS_SYSINFO` | `struct sysinfo *info` | Populates total RAM, free RAM, uptime in seconds, and active process count. |
| **`158`** | `SYS_ARCH_PRCTL` | `int code, unsigned long addr` | Sets Thread Local Storage (TLS) by updating `MSR_FS_BASE` (`0xC0000100`). |
| **`186`** | `SYS_GETTID` | *none* | Returns thread ID (`proc->pid`). |
| **`202`** | `SYS_FUTEX` | `uint32_t *uaddr, int op, uint32_t val, ...` | Fast user-space sleeping and waking synchronization primitive (`FUTEX_WAIT`, `FUTEX_WAKE`). |
| **`217`** | `SYS_GETDENTS64` | `int fd, struct linux_dirent64 *dirp, size_t count` | Enumerates directory entries from open directory file descriptors. |
| **`218`** | `SYS_SET_TID_ADDRESS`| `int *tidptr` | Registers thread clear address upon exit; returns thread TID. |
| **`228`** | `SYS_CLOCK_GETTIME`| `clockid_t clk_id, struct timespec *tp` | Returns monotonic and realtime timestamps with nanosecond precision. |
| **`231`** | `SYS_EXIT_GROUP` | `int status` | Terminates process or cleanly halts machine if PID 1. |
| **`257`** | `SYS_OPENAT` | `int dirfd, const char *path, int flags, int mode` | Relative / absolute file open via VFS. |
| **`262`** | `SYS_NEWFSTATAT` | `int dirfd, const char *path, struct stat *statbuf, int flags` | Relative / absolute stat via VFS. |
| **Default** | *Unhandled* | *any* | Evaluates to `-ENOSYS` (-38). |
