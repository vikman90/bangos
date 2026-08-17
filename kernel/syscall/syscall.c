#include "syscall.h"
#include "drivers/uart.h"
#include "drivers/keyboard.h"
#include "mm/memory.h"
#include "mm/vmm.h"
#include "process/process.h"
#include "lib/kstring.h"

static uint64_t current_brk = 0x600000000000ULL;
static uint64_t mapped_brk_page = 0x600000000000ULL;
static uint64_t kernel_boot_tsc = 0;

#define TSC_FREQ_HZ 1000000000ULL // 1.0 GHz baseline TSC frequency for calibration

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int64_t do_syscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, context_frame_t *frame) {
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
            uint64_t addr = arg1;
            size_t len = (size_t)arg2;
            int prot = (int)arg3;
            int flags = (int)arg4;

            if (len == 0) return -22; // -EINVAL

            process_t *proc = process_get_current();
            if (!proc) return -12; // -ENOMEM

            size_t aligned_len = (len + PAGE_SIZE - 1) & ~0xFFFULL;
            uint64_t virt_addr;

            if (flags & VMA_MAP_FIXED) {
                if (addr == 0 || (addr & 0xFFF)) return -22; // -EINVAL: unaligned address
                virt_addr = addr;
            } else if (addr != 0 && (addr & 0xFFF) == 0 && addr >= 0x40000000ULL && vma_find(proc, addr) == NULL) {
                virt_addr = addr;
            } else {
                virt_addr = vmm_find_free_area(proc, aligned_len);
            }

            uint32_t vma_prot = 0;
            if (prot & 0x1) vma_prot |= VMA_PROT_READ;
            if (prot & 0x2) vma_prot |= VMA_PROT_WRITE;
            if (prot & 0x4) vma_prot |= VMA_PROT_EXEC;

            uint32_t vma_flags = VMA_MAP_ANONYMOUS;
            if (flags & 0x01) vma_flags |= VMA_MAP_SHARED;
            if (flags & 0x02) vma_flags |= VMA_MAP_PRIVATE;
            if (flags & 0x10) vma_flags |= VMA_MAP_FIXED;

            vm_area_t *vma = vma_create(proc, virt_addr, virt_addr + aligned_len, vma_prot, vma_flags);
            if (!vma) return -12; // -ENOMEM

            size_t num_pages = aligned_len / PAGE_SIZE;
            void *phys = alloc_pages(num_pages);
            if (phys) {
                map_user_pages(virt_addr, (uint64_t)phys, num_pages);
            }

            return (int64_t)virt_addr;
        }

        case SYS_MPROTECT: {
            uint64_t addr = arg1;
            size_t len = (size_t)arg2;
            int prot = (int)arg3;
            if (len == 0 || (addr & 0xFFF)) return -22; // -EINVAL

            process_t *proc = process_get_current();
            if (!proc) return -12;

            size_t aligned_len = (len + PAGE_SIZE - 1) & ~0xFFFULL;
            return vma_protect(proc, addr, addr + aligned_len, prot);
        }

        case SYS_MUNMAP: {
            uint64_t addr = arg1;
            size_t len = (size_t)arg2;
            if (len == 0 || (addr & 0xFFF)) return -22; // -EINVAL

            process_t *proc = process_get_current();
            if (!proc) return -12;

            size_t aligned_len = (len + PAGE_SIZE - 1) & ~0xFFFULL;
            return vma_remove(proc, addr, addr + aligned_len);
        }

        case SYS_RT_SIGACTION:
        case SYS_RT_SIGPROCMASK: {
            return 0; // Success
        }

        case SYS_ARCH_PRCTL: {
            int code = (int)arg1;
            uint64_t addr = arg2;

            if (code == ARCH_SET_FS) {
                process_t *proc = process_get_current();
                if (proc) {
                    proc->fs_base = addr;
                }
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
            if (new_brk > mapped_brk_page) {
                uint64_t target_page = (new_brk + PAGE_SIZE - 1) & ~0xFFFULL;
                size_t num_pages = (target_page - mapped_brk_page) / PAGE_SIZE;
                void *phys = alloc_pages(num_pages);
                if (phys) {
                    map_user_pages(mapped_brk_page, (uint64_t)phys, num_pages);
                    mapped_brk_page = target_page;
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

        case SYS_SCHED_YIELD: {
            schedule_yield();
            return 0;
        }

        case SYS_GETPID: {
            process_t *proc = process_get_current();
            return proc ? (int64_t)proc->tgid : 1;
        }

        case SYS_GETTID: {
            process_t *proc = process_get_current();
            return proc ? (int64_t)proc->pid : 1;
        }

        case SYS_FORK:
        case SYS_VFORK: {
            return (int64_t)process_fork(frame);
        }

        case SYS_CLONE: {
            unsigned long flags = (unsigned long)arg1;
            void *child_stack = (void *)arg2;
            int *ptid = (int *)arg3;
            int *ctid = (int *)arg4;
            void *newtls = (void *)frame->r8; // 5th syscall argument in user registers
            return (int64_t)process_clone(flags, child_stack, ptid, ctid, newtls, frame);
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

        case SYS_FUTEX: {
            uint32_t *uaddr = (uint32_t *)arg1;
            int op = (int)arg2 & ~128; // Mask out FUTEX_PRIVATE_FLAG
            uint32_t val = (uint32_t)arg3;
            if (op == FUTEX_WAIT) {
                return (int64_t)futex_wait(uaddr, val);
            } else if (op == FUTEX_WAKE) {
                return (int64_t)futex_wake(uaddr, (int)val);
            }
            return -22; // -EINVAL
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

            uint64_t total_ticks = ((uint64_t)req->tv_sec * 100) +
                                   (((uint64_t)req->tv_nsec + 9999999ULL) / 10000000ULL);
            if (total_ticks == 0) total_ticks = 1;

            process_t *proc = process_get_current();
            if (proc) {
                proc->sleep_ticks_remaining = total_ticks;
                proc->state = PROCESS_STATE_SLEEPING;
                while (proc->sleep_ticks_remaining > 0) {
                    __asm__ volatile ("sti; hlt");
                }
            } else {
                uint64_t target_cycles = ((uint64_t)req->tv_sec * TSC_FREQ_HZ) +
                                         ((uint64_t)req->tv_nsec * TSC_FREQ_HZ / 1000000000ULL);
                uint64_t start_tsc = rdtsc();
                while ((rdtsc() - start_tsc) < target_cycles) {
                    __asm__ volatile ("pause");
                }
            }

            if (rem) {
                rem->tv_sec = 0;
                rem->tv_nsec = 0;
            }
            return 0;
        }

        case SYS_SET_TID_ADDRESS: {
            process_t *proc = process_get_current();
            if (proc) {
                proc->clear_child_tid = (int *)arg1;
                return (int64_t)proc->pid;
            }
            return 1;
        }

        case SYS_EXIT:
        case SYS_EXIT_GROUP: {
            int code = (int)arg1;
            kprintf("[Syscall] Process PID=%d called exit(%d)\n",
                    process_get_current() ? process_get_current()->pid : -1, code);
            process_exit(code);
            return 0;
        }

        default:
            kprintf("[Syscall Warning] Unhandled Syscall #%u\n", (uint32_t)sys_num);
            return -38;
    }
}
