#include "tui.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    tui_print_header("BangOS System Information & Hardware Report (Standalone ELF)");

    struct utsname u;
    if (uname(&u) == 0) {
        printf(ANSI_BOLD ANSI_CYAN "[Operating System Info]" ANSI_RESET "\n");
        printf("  - System Name:    " ANSI_GREEN "%s" ANSI_RESET "\n", u.sysname);
        printf("  - Release:        %s\n", u.release);
        printf("  - Kernel Version: %s\n", u.version);
        printf("  - Architecture:   %s\n", u.machine);
        printf("  - Hostname:       %s\n", u.nodename);
    } else {
        printf(ANSI_RED "Error: uname() syscall failed.\n" ANSI_RESET);
    }

    tui_print_divider();

    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        unsigned long total_mb = s.totalram / (1024 * 1024);
        unsigned long free_mb = s.freeram / (1024 * 1024);
        unsigned long used_mb = (s.totalram > s.freeram) ? (s.totalram - s.freeram) / (1024 * 1024) : 0;

        printf(ANSI_BOLD ANSI_CYAN "[Memory & Performance Metrics]" ANSI_RESET "\n");
        printf("  - Physical RAM:   %lu MB Total | %lu MB Used | " ANSI_GREEN "%lu MB Free" ANSI_RESET "\n",
               total_mb, used_mb, free_mb);
        printf("  - System Uptime:  %lu seconds\n", s.uptime);
        printf("  - Active Procs:   %u (Current PID: %d)\n", s.procs, getpid());
    } else {
        printf(ANSI_RED "Error: sysinfo() syscall failed.\n" ANSI_RESET);
    }

    tui_print_divider();

    printf(ANSI_BOLD ANSI_CYAN "[Mounted Storage & Filesystems]" ANSI_RESET "\n");
    printf("  - Primary Drive:  /dev/ata0 (32 MB ATA PIO Block Storage)\n");
    printf("  - Root FS (/):    In-Memory TarFS USTAR Ramdisk (/bin)\n");
    printf("  - Persistent FS:  ext2 mounted at /mnt/ext2 (Read-Write)\n");

    tui_print_divider();
    tui_pause();
    return 0;
}
