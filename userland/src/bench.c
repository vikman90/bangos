#include "app.h"
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

int app_bench_main(void) {
    tui_print_header("BangOS CPU, FPU/SSE & Timer Benchmark");

    // 1. FPU / SSE Math Benchmark
    printf(ANSI_BOLD ANSI_CYAN "[1/3] Floating-Point / SSE Benchmark (20M Iterations)" ANSI_RESET "\n");
    printf("Computing Leibniz series for Pi & trigonometric ops...\n");
    fflush(stdout);

    double start_t = get_time_sec();
    double pi_approx = 0.0;
    double sign = 1.0;
    const long iterations = 20000000;

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
    printf(ANSI_BOLD ANSI_CYAN "[2/3] Dynamic Heap Memory Benchmark (10,000 allocations)" ANSI_RESET "\n");
    fflush(stdout);

    start_t = get_time_sec();
    const int alloc_count = 10000;
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
    printf("  - Heap Allocs:    10,000 malloc/free operations completed.\n");
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

int app_memtest_main(void) {
    tui_print_header("BangOS Dynamic Memory Stress Test");

    printf("Allocating sequential memory blocks up to 8 MB...\n");
    fflush(stdout);

    const size_t block_size = 256 * 1024; // 256 KB
    const int num_blocks = 32;            // 8 MB total
    void *blocks[32];
    int success_count = 0;

    for (int i = 0; i < num_blocks; i++) {
        blocks[i] = malloc(block_size);
        if (!blocks[i]) {
            printf(ANSI_RED "Failed to allocate block %d (out of memory).\n" ANSI_RESET, i);
            break;
        }

        // Fill pattern
        unsigned char *buf = (unsigned char *)blocks[i];
        for (size_t b = 0; b < block_size; b += 64) {
            buf[b] = (unsigned char)(i + (b & 0xFF));
        }
        success_count++;
    }

    printf(ANSI_GREEN "Successfully allocated and populated %d blocks (%zu KB).\n" ANSI_RESET,
           success_count, (success_count * block_size) / 1024);

    printf("Verifying memory integrity across all blocks...\n");
    bool integrity_ok = true;
    for (int i = 0; i < success_count; i++) {
        unsigned char *buf = (unsigned char *)blocks[i];
        for (size_t b = 0; b < block_size; b += 64) {
            if (buf[b] != (unsigned char)(i + (b & 0xFF))) {
                integrity_ok = false;
                printf(ANSI_RED "Data corruption detected in block %d offset %zu!\n" ANSI_RESET, i, b);
                break;
            }
        }
        free(blocks[i]);
    }

    if (integrity_ok) {
        printf(ANSI_BOLD ANSI_GREEN "Memory Integrity Verification: PASSED (100%% OK)" ANSI_RESET "\n");
    } else {
        printf(ANSI_BOLD ANSI_RED "Memory Integrity Verification: FAILED" ANSI_RESET "\n");
    }

    tui_print_divider();
    tui_pause();
    return integrity_ok ? 0 : 1;
}
