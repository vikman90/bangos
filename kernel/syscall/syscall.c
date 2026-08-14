#include "syscall.h"
#include "drivers/uart.h"
#include "drivers/keyboard.h"
#include "mm/memory.h"
#include "process/process.h"
#include "lib/kstring.h"

static uint64_t current_brk = 0x800000000000ULL;
static uint64_t kernel_boot_tsc = 0;

#define TSC_FREQ_HZ 1000000000ULL // 1.0 GHz baseline TSC frequency for calibration

static inline void write_msr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int64_t do_syscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;

    if (kernel_boot_tsc == 0) {
        kernel_boot_tsc = rdtsc();
    }

    switch (sys_num) {
        case SYS_READ: {
            int fd = (int)arg1;
            char *buf = (char *)arg2;
            size_t count = (size_t)arg3;

            if (fd != 0 || count == 0 || !buf) return -1;

            size_t bytes_read = 0;
            while (bytes_read < count) {
                char ch = console_getchar();
                buf[bytes_read++] = ch;
                uart_putc(ch);
                if (ch == '\n') break;
            }
            return (int64_t)bytes_read;
        }

        case SYS_WRITE: {
            int fd = (int)arg1;
            const char *buf = (const char *)arg2;
            size_t count = (size_t)arg3;

            if ((fd == 1 || fd == 2) && buf) {
                for (size_t i = 0; i < count; i++) {
                    uart_putc(buf[i]);
                }
                return (int64_t)count;
            }
            return -1;
        }

        case SYS_WRITEV: {
            int fd = (int)arg1;
            const struct iovec *iov = (const struct iovec *)arg2;
            int iovcnt = (int)arg3;

            if ((fd == 1 || fd == 2) && iov && iovcnt > 0) {
                int64_t total_written = 0;
                for (int i = 0; i < iovcnt; i++) {
                    if (iov[i].iov_base && iov[i].iov_len > 0) {
                        const char *p = (const char *)iov[i].iov_base;
                        for (size_t j = 0; j < iov[i].iov_len; j++) {
                            uart_putc(p[j]);
                        }
                        total_written += iov[i].iov_len;
                    }
                }
                return total_written;
            }
            return -1;
        }

        case SYS_POLL: {
            return 1;
        }

        case SYS_MMAP: {
            size_t len = (size_t)arg2;
            if (len == 0) return -22; // -EINVAL
            size_t num_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
            void *phys = alloc_pages(num_pages);
            if (!phys) return -12; // -ENOMEM
            uint64_t virt_addr = current_brk;
            current_brk += num_pages * PAGE_SIZE;
            map_user_pages(virt_addr, (uint64_t)phys, num_pages);
            return (int64_t)virt_addr;
        }

        case SYS_MPROTECT:
        case SYS_MUNMAP:
        case SYS_RT_SIGACTION:
        case SYS_RT_SIGPROCMASK: {
            return 0; // Success
        }

        case SYS_ARCH_PRCTL: {
            int code = (int)arg1;
            uint64_t addr = arg2;

            if (code == ARCH_SET_FS) {
                write_msr(0xC0000100, addr); // Set MSR_FS_BASE
                return 0;
            } else if (code == ARCH_SET_GS) {
                write_msr(0xC0000101, addr); // Set MSR_GS_BASE
                return 0;
            }
            return -22; // -EINVAL
        }

        case SYS_BRK: {
            uint64_t new_brk = arg1;
            if (new_brk == 0) {
                return (int64_t)current_brk;
            }
            if (new_brk > current_brk) {
                size_t num_pages = (new_brk - current_brk + PAGE_SIZE - 1) / PAGE_SIZE;
                void *phys = alloc_pages(num_pages);
                if (phys) {
                    map_user_pages(current_brk, (uint64_t)phys, num_pages);
                    current_brk = new_brk;
                }
            } else {
                current_brk = new_brk;
            }
            return (int64_t)current_brk;
        }

        case SYS_IOCTL: {
            int fd = (int)arg1;
            uint64_t cmd = arg2;
            void *arg = (void *)arg3;

            if ((fd == 0 || fd == 1 || fd == 2) && cmd == 0x5413 && arg) { // TIOCGWINSZ
                struct winsize *ws = (struct winsize *)arg;
                ws->ws_row = 24;
                ws->ws_col = 80;
                ws->ws_xpixel = 0;
                ws->ws_ypixel = 0;
                return 0;
            }
            return 0;
        }

        case SYS_LSEEK: {
            return -29; // -ESPIPE
        }

        case SYS_GETPID:
        case SYS_GETTID: {
            process_t *proc = process_get_current();
            return proc ? (int64_t)proc->pid : 1;
        }

        case SYS_CLONE:
        case SYS_FORK:
        case SYS_VFORK: {
            process_fork(0, 0, 0);
            return 0; // Return 0 to execute child branch
        }

        case SYS_EXECVE: {
            const char *path = (const char *)arg1;
            char *const *argv = (char *const *)arg2;
            char *const *envp = (char *const *)arg3;
            return (int64_t)process_execve(path, argv, envp);
        }

        case SYS_WAIT4: {
            int pid = (int)arg1;
            int *status = (int *)arg2;
            return (int64_t)process_wait4(pid, status);
        }

        case SYS_UNAME: {
            struct utsname *u = (struct utsname *)arg1;
            if (!u) return -14; // -EFAULT

            kmemset(u, 0, sizeof(struct utsname));
            kstrncpy(u->sysname, "BangOS", sizeof(u->sysname));
            kstrncpy(u->nodename, "bangos", sizeof(u->nodename));
            kstrncpy(u->release, "0.2.0", sizeof(u->release));
            kstrncpy(u->version, "#1 SMP Bare-Metal x86_64 UEFI", sizeof(u->version));
            kstrncpy(u->machine, "x86_64", sizeof(u->machine));
            kstrncpy(u->domainname, "local", sizeof(u->domainname));
            return 0;
        }

        case SYS_SYSINFO: {
            struct sysinfo *info = (struct sysinfo *)arg1;
            if (!info) return -14; // -EFAULT

            kmemset(info, 0, sizeof(struct sysinfo));
            uint64_t now_cycles = rdtsc() - kernel_boot_tsc;
            info->uptime = (unsigned long)(now_cycles / TSC_FREQ_HZ);
            info->totalram = mm_get_total_bytes();
            info->freeram = mm_get_free_bytes();
            info->procs = 1;
            info->mem_unit = 1;
            return 0;
        }

        case SYS_CLOCK_GETTIME: {
            int clk_id = (int)arg1;
            struct timespec *ts = (struct timespec *)arg2;
            (void)clk_id;

            if (!ts) return -14; // -EFAULT

            uint64_t now_cycles = rdtsc() - kernel_boot_tsc;
            ts->tv_sec = (int64_t)(now_cycles / TSC_FREQ_HZ);
            ts->tv_nsec = (int64_t)((now_cycles % TSC_FREQ_HZ) * (1000000000ULL / TSC_FREQ_HZ));
            return 0;
        }

        case SYS_NANOSLEEP: {
            const struct timespec *req = (const struct timespec *)arg1;
            struct timespec *rem = (struct timespec *)arg2;

            if (!req) return -14; // -EFAULT
            if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) {
                return -22; // -EINVAL
            }

            uint64_t target_cycles = ((uint64_t)req->tv_sec * TSC_FREQ_HZ) +
                                     ((uint64_t)req->tv_nsec * TSC_FREQ_HZ / 1000000000ULL);
            uint64_t start_tsc = rdtsc();
            while ((rdtsc() - start_tsc) < target_cycles) {
                __asm__ volatile ("pause");
            }

            if (rem) {
                rem->tv_sec = 0;
                rem->tv_nsec = 0;
            }
            return 0;
        }

        case SYS_SET_TID_ADDRESS: {
            return 1;
        }

        case SYS_EXIT:
        case SYS_EXIT_GROUP: {
            int code = (int)arg1;
            process_exit(code);
            return 0;
        }

        default:
            kprintf("[Syscall Warning] Unhandled Syscall #%u\n", (uint32_t)sys_num);
            return -38;
    }
}
