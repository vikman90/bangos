#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

static double get_time_sec(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    tui_print_header("BangOS CPU, FPU/SSE & Memory Benchmark (Standalone ELF)");

    // 1. FPU / SSE Math Benchmark
    printf(ANSI_BOLD ANSI_CYAN "[1/3] Floating-Point / SSE Benchmark (2M Iterations)" ANSI_RESET "\n");
    printf("Computing Leibniz series for Pi & trigonometric ops...\n");
    fflush(stdout);

    double start_t = get_time_sec();
    double pi_approx = 0.0;
    double sign = 1.0;
    const long iterations = 2000000;

    for (long i = 0; i < iterations; i++) {
        pi_approx += sign / (2.0 * (double)i + 1.0);
        sign = -sign;
    }
    pi_approx *= 4.0;
    double end_t = get_time_sec();
    double fpu_time = end_t - start_t;
    if (fpu_time < 0.00001) fpu_time = 0.00001;

    double mops = ((double)iterations / fpu_time) / 1e6;
    printf("  - Calculated Pi:  %.10f (Error: %.10e)\n", pi_approx, fabs(pi_approx - 3.141592653589793));
    printf("  - Elapsed Time:   " ANSI_GREEN "%.4f s" ANSI_RESET " (" ANSI_BOLD "%.2f Mops/s" ANSI_RESET ")\n\n",
           fpu_time, mops);

    // 2. Dynamic Memory (malloc / free) Benchmark
    printf(ANSI_BOLD ANSI_CYAN "[2/3] Dynamic Heap Memory Benchmark (1,000 allocations)" ANSI_RESET "\n");
    fflush(stdout);

    start_t = get_time_sec();
    const int alloc_count = 1000;
    void *ptrs[100];
    for (int cycle = 0; cycle < alloc_count / 100; cycle++) {
        for (int i = 0; i < 100; i++) {
            ptrs[i] = malloc(128 + (i * 16));
            if (ptrs[i]) {
                *(volatile int *)ptrs[i] = i * 42;
            }
        }
        for (int i = 0; i < 100; i++) {
            free(ptrs[i]);
        }
    }
    end_t = get_time_sec();
    double mem_time = end_t - start_t;
    printf("  - Heap Allocs:    1,000 malloc/free operations completed.\n");
    printf("  - Elapsed Time:   " ANSI_GREEN "%.4f s" ANSI_RESET "\n\n", mem_time);

    // 3. Nanosleep Precision Test
    printf(ANSI_BOLD ANSI_CYAN "[3/3] Precision Nanosleep Test (500 ms pause)" ANSI_RESET "\n");
    fflush(stdout);

    struct timespec req = { .tv_sec = 0, .tv_nsec = 500000000L };
    start_t = get_time_sec();
    nanosleep(&req, NULL);
    end_t = get_time_sec();
    double sleep_time = end_t - start_t;
    printf("  - Requested Sleep: 0.5000 s\n");
    printf("  - Measured Sleep:  " ANSI_GREEN "%.4f s" ANSI_RESET "\n", sleep_time);

    tui_print_divider();
    tui_pause();
    return 0;
}
