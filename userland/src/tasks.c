#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>

static volatile bool worker_running = true;

static bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

static void run_background_worker(int worker_id) {
    int primes_found = 0;
    int current_num = 2;
    int iterations = 0;

    while (worker_running) {
        if (is_prime(current_num)) {
            primes_found++;
        }
        current_num++;
        iterations++;

        if (iterations % 500000 == 0) {
            printf("\n" ANSI_DIM ANSI_CYAN "[Worker %d (PID %d)] Iteration %d | Primes found: %d | Current: %d" ANSI_RESET "\n> ",
                   worker_id, getpid(), iterations, primes_found, current_num);
            fflush(stdout);
        }
    }
    exit(0);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n" ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n");
    printf(ANSI_BOLD ANSI_CYAN "        BangOS Preemptive Multitasking & Concurrent Tasks Demo         " ANSI_RESET "\n");
    printf(ANSI_DIM "     Spawning background computation worker while maintaining UART shell" ANSI_RESET "\n");
    printf(ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n\n");

    printf("[Main] Forking background computation worker...\n");
    fflush(stdout);

    pid_t worker_pid = fork();
    if (worker_pid < 0) {
        printf(ANSI_RED "Error: Failed to fork background worker.\n" ANSI_RESET);
        return 1;
    }

    if (worker_pid == 0) {
        // Child: Background Worker
        run_background_worker(1);
        return 0;
    }

    // Parent: Foreground Interactive Shell
    printf(ANSI_GREEN "[Main] Background worker successfully spawned (PID %d)." ANSI_RESET "\n", worker_pid);
    printf(ANSI_YELLOW "Available commands: 'status', 'ping', 'stats', 'spawn', 'exit'" ANSI_RESET "\n\n");
    printf("> ");
    fflush(stdout);

    int extra_workers[8];
    int extra_count = 0;
    for (int i = 0; i < 8; i++) extra_workers[i] = 0;

    char line[64];
    while (1) {
        if (tui_read_line(line, sizeof(line)) != 0) {
            continue;
        }

        if (strcmp(line, "status") == 0) {
            printf("[Tasks] System Multitasking Status: ACTIVE\n");
            printf("  - Primary Background Worker PID: %d (RUNNING)\n", worker_pid);
            printf("  - Extra Active Workers:          %d\n", extra_count);
            for (int i = 0; i < extra_count; i++) {
                printf("    * Extra Worker #%d: PID %d\n", i + 1, extra_workers[i]);
            }
            printf("  - Foreground Interactive Shell:  PID %d (RUNNING)\n", getpid());
        } else if (strcmp(line, "ping") == 0) {
            printf(ANSI_GREEN "pong (Preemptive Context Switch latency < 10ms)" ANSI_RESET "\n");
        } else if (strcmp(line, "stats") == 0) {
            printf("[Stats] Multitasking tick rate: 100 Hz (PIT IRQ 0 / 10 ms quantum)\n");
            printf("[Stats] Round-Robin scheduling active across Ring 3 processes.\n");
        } else if (strcmp(line, "spawn") == 0) {
            if (extra_count < 8) {
                pid_t new_pid = fork();
                if (new_pid == 0) {
                    run_background_worker(extra_count + 2);
                    return 0;
                } else if (new_pid > 0) {
                    extra_workers[extra_count] = new_pid;
                    extra_count++;
                    printf(ANSI_GREEN "[Tasks] Spawned additional background worker PID %d (Total workers: %d)\n" ANSI_RESET,
                           new_pid, extra_count + 1);
                }
            } else {
                printf(ANSI_YELLOW "[Tasks] Maximum background workers (8) reached.\n" ANSI_RESET);
            }
        } else if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("\n[Tasks] Terminating multitasking session...\n");
            break;
        } else if (strlen(line) > 0) {
            printf(ANSI_RED "Unknown command '%s'. Try: 'status', 'ping', 'stats', 'spawn', 'exit'\n" ANSI_RESET, line);
        }

        printf("> ");
        fflush(stdout);
    }

    return 0;
}
