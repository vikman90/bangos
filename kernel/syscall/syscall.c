#include "syscall.h"
#include "drivers/uart.h"
#include "drivers/keyboard.h"
#include "mm/memory.h"
#include <string.h>

static uint64_t current_brk = 0x800000000000ULL;

static inline void write_msr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

int64_t do_syscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;

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
        case SYS_MUNMAP: {
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

            if ((fd == 1 || fd == 2) && cmd == 0x5413 && arg) { // TIOCGWINSZ
                struct winsize *ws = (struct winsize *)arg;
                ws->ws_row = 24;
                ws->ws_col = 80;
                ws->ws_xpixel = 0;
                ws->ws_ypixel = 0;
                return 0;
            }
            return -25; // -ENOTTY
        }

        case SYS_LSEEK: {
            return -29; // -ESPIPE
        }

        case SYS_SET_TID_ADDRESS: {
            return 1;
        }

        case SYS_EXIT:
        case SYS_EXIT_GROUP: {
            int code = (int)arg1;
            kprintf("\n[Kernel] Process exited with status code: %d\n", code);
            kprintf("[Kernel] System halting cleanly.\n");
            while (1) {
                __asm__ volatile ("cli; hlt");
            }
            return 0;
        }

        default:
            kprintf("[Syscall Warning] Unhandled Syscall #%u\n", (uint32_t)sys_num);
            return -38;
    }
}
