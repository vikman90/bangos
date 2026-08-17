#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>

static void print_system_banner(void) {
    struct utsname u;
    struct sysinfo s;

    uname(&u);
    sysinfo(&s);

    unsigned long total_mb = s.totalram / (1024 * 1024);
    unsigned long free_mb = s.freeram / (1024 * 1024);

    printf("\n" ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n");
    printf(ANSI_BOLD ANSI_CYAN "        %s (x86_64) - Bare Metal Kernel v%s        " ANSI_RESET "\n", u.sysname, u.release);
    printf(ANSI_DIM "     PID: %d (init) | RAM: %lu MB Total (%lu MB Free) | Uptime: %lu s" ANSI_RESET "\n",
           getpid(), total_mb, free_mb, s.uptime);
    printf(ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n\n");
}

static void show_menu(void) {
    printf(ANSI_BOLD "Available Standalone Applications (Multi-ELF Ramdisk):" ANSI_RESET "\n\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[1]" ANSI_RESET " Geometric Calculator           (execve /bin/calc)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[2]" ANSI_RESET " System Information & Uname     (execve /bin/sysinfo)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[3]" ANSI_RESET " CPU FPU/SSE & Timer Benchmark  (execve /bin/bench)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[4]" ANSI_RESET " Preemptive Multitasking Tasks  (execve /bin/tasks)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[5]" ANSI_RESET " Multithreading & Mutex Sync    (execve /bin/threads)\n");
    printf("  " ANSI_BOLD ANSI_GREEN "[6]" ANSI_RESET " Run Specification Test Suites  (execve /bin/test_*)\n");
    printf("  " ANSI_BOLD ANSI_RED   "[7]" ANSI_RESET " Shutdown / Halt System         (exit)\n\n");
    printf(ANSI_BOLD ANSI_YELLOW "Select an option [1-7]: " ANSI_RESET);
    fflush(stdout);
}

static int launch_program(const char *path, const char *name) {
    printf(ANSI_DIM "Spawning process '%s' via fork() + execve()...\n" ANSI_RESET, path);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char *argv[] = { (char *)name, NULL };
        execve(path, argv, NULL);
        printf(ANSI_RED "Error: execve('%s') failed.\n" ANSI_RESET, path);
        exit(127);
    } else if (pid > 0) {
        // Parent process
        int status = 0;
        waitpid(pid, &status, 0);
        return status;
    } else {
        printf(ANSI_RED "Error: fork() failed.\n" ANSI_RESET);
        return -1;
    }
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
            launch_program("/bin/calc", "calc");
        } else if (strcmp(line, "2") == 0 || strcmp(line, "sysinfo") == 0) {
            launch_program("/bin/sysinfo", "sysinfo");
        } else if (strcmp(line, "3") == 0 || strcmp(line, "bench") == 0) {
            launch_program("/bin/bench", "bench");
        } else if (strcmp(line, "4") == 0 || strcmp(line, "tasks") == 0) {
            launch_program("/bin/tasks", "tasks");
        } else if (strcmp(line, "5") == 0 || strcmp(line, "threads") == 0) {
            launch_program("/bin/threads", "threads");
        } else if (strcmp(line, "6") == 0 || strcmp(line, "tests") == 0 || strcmp(line, "test") == 0) {
            launch_program("/bin/test_syscall_safety", "test_syscall_safety");
            launch_program("/bin/test_vmm_demand", "test_vmm_demand");
            launch_program("/bin/test_process_lifecycle", "test_process_lifecycle");
            tui_pause();
        } else if (strcmp(line, "7") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("\n" ANSI_BOLD ANSI_RED "Shutting down BangOS system cleanly..." ANSI_RESET "\n");
            fflush(stdout);
            exit(0);
        } else if (strlen(line) > 0) {
            printf(ANSI_RED "Invalid selection '%s'. Please choose an option between 1 and 7.\n" ANSI_RESET, line);
            tui_pause();
        }
    }

    return 0;
}
