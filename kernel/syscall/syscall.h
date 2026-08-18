#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "arch/x86_64/idt.h"

#define SYS_READ            0
#define SYS_WRITE           1
#define SYS_OPEN            2
#define SYS_CLOSE           3
#define SYS_STAT            4
#define SYS_FSTAT           5
#define SYS_POLL            7
#define SYS_LSEEK           8
#define SYS_MMAP            9
#define SYS_MPROTECT        10
#define SYS_MUNMAP          11
#define SYS_BRK             12
#define SYS_RT_SIGACTION    13
#define SYS_RT_SIGPROCMASK  14
#define SYS_IOCTL           16
#define SYS_WRITEV          20
#define SYS_SCHED_YIELD     24
#define SYS_NANOSLEEP       35
#define SYS_GETPID          39
#define SYS_CLONE           56
#define SYS_FORK            57
#define SYS_VFORK           58
#define SYS_EXECVE          59
#define SYS_EXIT            60
#define SYS_WAIT4           61
#define SYS_UNAME           63
#define SYS_SYSINFO         99
#define SYS_ARCH_PRCTL      158
#define SYS_GETTID          186
#define SYS_FUTEX           202
#define SYS_GETDENTS64      217
#define SYS_SET_TID_ADDRESS 218
#define SYS_CLOCK_GETTIME   228
#define SYS_EXIT_GROUP      231
#define SYS_OPENAT          257
#define SYS_NEWFSTATAT      262

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    int64_t  st_atime_sec;
    uint64_t st_atime_nsec;
    int64_t  st_mtime_sec;
    uint64_t st_mtime_nsec;
    int64_t  st_ctime_sec;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

/* Clone Flags */
#define CLONE_VM            0x00000100
#define CLONE_FS            0x00000200
#define CLONE_FILES         0x00000400
#define CLONE_SIGHAND       0x00000800
#define CLONE_THREAD        0x00010000
#define CLONE_SETTLS        0x00080000
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_CHILD_SETTID  0x01000000

/* Futex Operations */
#define FUTEX_WAIT          0
#define FUTEX_WAKE          1
#define FUTEX_PRIVATE_FLAG  128
#define FUTEX_CLOCK_REALTIME 256

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct sysinfo {
    unsigned long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char __f[256 - 2 * sizeof(unsigned long) - sizeof(unsigned int)];
};

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

void    syscall_init_msrs(void);
int64_t do_syscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, context_frame_t *frame);

#endif /* SYSCALL_H */
