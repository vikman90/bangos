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
| `8` | `SYS_LSEEK` | `fd`, `offset`, `whence` | Returns `-ESPIPE` (-29) for serial streams |
| `9` | `SYS_MMAP` | `addr`, `len`, `prot`, ... | Allocates physical/virtual pages dynamically in user space |
| `10` | `SYS_MPROTECT` | `addr`, `len`, `prot` | Returns `0` (success) |
| `11` | `SYS_MUNMAP` | `addr`, `len` | Returns `0` (success) |
| `12` | `SYS_BRK` | `brk_addr` | Queries or expands user process heap boundary |
| `16` | `SYS_IOCTL` | `fd`, `cmd`, `arg` | Handles `TIOCGWINSZ` (`0x5413`) reporting an 80x24 terminal |
| `20` | `SYS_WRITEV` | `fd`, `iov`, `iovcnt` | Writes formatted vector buffers produced by `printf()` / `vfprintf()` |
| `60` | `SYS_EXIT` | `status` | Prints exit status and cleanly halts the CPU |
| `158` | `SYS_ARCH_PRCTL` | `code`, `addr` | Configures TLS (Thread Local Storage) by writing `MSR_FS_BASE` (`0xC0000100`) |
| `218` | `SYS_SET_TID_ADDRESS` | `tidptr` | Returns `1` (main process TID) |
| `231` | `SYS_EXIT_GROUP` | `status` | Prints exit status and cleanly halts the system |
