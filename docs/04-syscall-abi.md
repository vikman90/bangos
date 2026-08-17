# 04 - System Call Interface (Linux x86_64 ABI)

BangOS implements the native Linux x86_64 system call interface via the hardware `syscall` / `sysret` instruction mechanism. This allows running unmodified standard C binaries compiled statically with `musl-gcc`.

---

## ⚡ MSR Configuration (`kernel/arch/x86_64/syscall_entry.s`)

The `syscall` and `sysret` instructions rely on Model Specific Registers (MSRs):

| MSR Address | MSR Name | Configured Value | Purpose |
| :--- | :--- | :--- | :--- |
| `0xC0000080` | `IA32_EFER` | `EFER.SCE = 1` | Enables `syscall`/`sysret` instructions |
| `0xC0000081` | `MSR_STAR` | `0x00100008` | Base CS/SS segment selectors for Kernel and User |
| `0xC0000082` | `MSR_LSTAR` | `&syscall_entry` | Ring 0 target RIP address when `syscall` is executed |
| `0xC0000084` | `MSR_SFMASK` | `0x200` | RFLAGS mask (clears IF bit upon entering syscall) |

---

## 📋 System V AMD64 ABI Register Convention

When userland executes `syscall`:
* **Syscall Number**: Register `RAX`.
* **Arguments**: `RDI` (1st), `RSI` (2nd), `RDX` (3rd), `R10` (4th), `R8` (5th), `R9` (6th).
* **Saved State**: `RCX` saves user `RIP`; `R11` saves user `RFLAGS`.
* **Return Value**: Returned in `RAX` (negative values represent Linux `-ERRNO` error codes).

---

## 🛠️ Implemented Syscalls Table (`kernel/syscall/syscall.c`)

| Syscall # | Name | Arguments | Behavior in BangOS |
| :--- | :--- | :--- | :--- |
| `0` | `SYS_READ` | `fd`, `buf`, `count` | Reads characters from UART serial console (`COM1`) |
| `1` | `SYS_WRITE` | `fd`, `buf`, `count` | Writes characters to UART serial console (`COM1`) |
| `7` | `SYS_POLL` | `fds`, `nfds`, `timeout` | Non-blocking poll for I/O readiness |
| `8` | `SYS_LSEEK` | `fd`, `offset`, `whence` | Returns `-ESPIPE` (-29) for serial streams |
| `9` | `SYS_MMAP` | `addr`, `len`, `prot`, `flags`, `fd`, `off` | Allocates virtual memory area (VMA) and memory pages dynamically in user space |
| `10` | `SYS_MPROTECT` | `addr`, `len`, `prot` | Modifies VMA permissions and updates page table protection bits |
| `11` | `SYS_MUNMAP` | `addr`, `len` | Unmaps virtual memory range, reclaims physical frames, and trims VMAs |

| `12` | `SYS_BRK` | `brk_addr` | Queries or expands user process heap boundary |
| `16` | `SYS_IOCTL` | `fd`, `cmd`, `arg` | Handles terminal attributes (`TIOCGWINSZ`, etc.) |
| `20` | `SYS_WRITEV` | `fd`, `iov`, `iovcnt` | Writes formatted vector buffers produced by `printf()` / `vfprintf()` |
| `24` | `SYS_SCHED_YIELD` | *none* | Yields the remaining CPU time quantum to the next ready task |
| `35` | `SYS_NANOSLEEP` | `req`, `rem` | High-precision sleep using calibrated timestamp counter (TSC) |
| `39` | `SYS_GETPID` | *none* | Returns current process PID |
| `56` | `SYS_CLONE` | `flags`, `stack`, `ptid`, `ctid`, `newtls` | Creates new execution context: processes or threads (`CLONE_VM`, `CLONE_THREAD`) |
| `57` | `SYS_FORK` | *none* | Creates child process context with independent address space |
| `58` | `SYS_VFORK` | *none* | Creates child process context |
| `59` | `SYS_EXECVE` | `path`, `argv`, `envp` | Replaces current process with new ELF from TarFS ramdisk |
| `60` | `SYS_EXIT` | `status` | Terminates process/thread, notifies parent via `wait4`, or halts system if PID 1 |
| `61` | `SYS_WAIT4` | `pid`, `status`, `opts` | Waits for child process termination and retrieves exit status code |
| `63` | `SYS_UNAME` | `buf` | Populates `utsname` (BangOS release, architecture, hostname) |
| `99` | `SYS_SYSINFO` | `info` | Populates `sysinfo` (total RAM, free RAM, uptime, active procs) |
| `158` | `SYS_ARCH_PRCTL` | `code`, `addr` | Configures TLS (Thread Local Storage) by writing `MSR_FS_BASE` (`0xC0000100`) |
| `186` | `SYS_GETTID` | *none* | Returns caller thread TID (Process ID / Thread ID) |
| `202` | `SYS_FUTEX` | `uaddr`, `op`, `val`, ... | Fast user-space locking primitive (`FUTEX_WAIT`, `FUTEX_WAKE`) |
| `218` | `SYS_SET_TID_ADDRESS` | `tidptr` | Returns process TID |
| `228` | `SYS_CLOCK_GETTIME` | `clk_id`, `tp` | Returns monotonic / realtime timestamp in seconds and nanoseconds |
| `231` | `SYS_EXIT_GROUP` | `status` | Terminates process or halts system if PID 1 |
