#include "app.h"
#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

static void print_system_banner(void) {
    struct utsname u;
    struct sysinfo s;

    uname(&u);
    sysinfo(&s);

    unsigned long total_mb = s.totalram / (1024 * 1024);
    unsigned long free_mb = s.freeram / (1024 * 1024);

    printf("\n" ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n");
    printf(ANSI_BOLD ANSI_CYAN "        %s (x86_64) - Bare Metal Kernel v%s        " ANSI_RESET "\n", u.sysname, u.release);
    printf(ANSI_DIM "     PID: %d | RAM: %lu MB Total (%lu MB Free) | Uptime: %lu s" ANSI_RESET "\n",
           getpid(), total_mb, free_mb, s.uptime);
    printf(ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n\n");
}

static void show_menu(void) {
    printf(ANSI_BOLD "Available Applications & System Utilities:" ANSI_RESET "\n\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[1]" ANSI_RESET " Geometric Calculator           (calc)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[2]" ANSI_RESET " System Information & Uname     (sysinfo)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[3]" ANSI_RESET " CPU FPU/SSE & Timer Benchmark  (bench)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[4]" ANSI_RESET " Dynamic Memory Stress Test     (memtest)\n");
    printf("  " ANSI_BOLD ANSI_RED   "[5]" ANSI_RESET " Shutdown / Halt System         (exit)\n\n");
    printf(ANSI_BOLD ANSI_YELLOW "Select an option [1-5]: " ANSI_RESET);
    fflush(stdout);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    while (1) {
        print_system_banner();
        show_menu();

        char line[64];
        if (tui_read_line(line, sizeof(line)) != 0) {
            continue;
        }

        if (strcmp(line, "1") == 0 || strcmp(line, "calc") == 0) {
            app_calc_main();
        } else if (strcmp(line, "2") == 0 || strcmp(line, "sysinfo") == 0) {
            app_sysinfo_main();
        } else if (strcmp(line, "3") == 0 || strcmp(line, "bench") == 0) {
            app_bench_main();
        } else if (strcmp(line, "4") == 0 || strcmp(line, "memtest") == 0) {
            app_memtest_main();
        } else if (strcmp(line, "5") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("\n" ANSI_BOLD ANSI_RED "Shutting down BangOS system cleanly..." ANSI_RESET "\n");
            fflush(stdout);
            exit(0);
        } else if (strlen(line) > 0) {
            printf(ANSI_RED "Invalid selection '%s'. Please choose an option between 1 and 5.\n" ANSI_RESET, line);
            tui_pause();
        }
    }

    return 0;
}
