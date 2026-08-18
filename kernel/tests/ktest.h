#ifndef KERNEL_TESTS_KTEST_H
#define KERNEL_TESTS_KTEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "drivers/uart.h"

__attribute__((visibility("hidden"))) extern int g_ktest_passed;
__attribute__((visibility("hidden"))) extern int g_ktest_failed;
__attribute__((visibility("hidden"))) extern const char *g_current_test_name;

#define KTEST_ASSERT(cond) do { \
    if (!(cond)) { \
        kprintf("[KTEST][FAIL] %s: Assertion '%s' failed at %s:%d\n", \
                g_current_test_name, #cond, __FILE__, __LINE__); \
        g_ktest_failed++; \
        return false; \
    } \
} while (0)

#define KTEST_ASSERT_EQ(a, b) do { \
    uint64_t _val_a = (uint64_t)(a); \
    uint64_t _val_b = (uint64_t)(b); \
    if (_val_a != _val_b) { \
        kprintf("[KTEST][FAIL] %s: Expected %s (%p) == %s (%p) at %s:%d\n", \
                g_current_test_name, #a, (void *)_val_a, #b, (void *)_val_b, __FILE__, __LINE__); \
        g_ktest_failed++; \
        return false; \
    } \
} while (0)

#define KTEST_ASSERT_NE(a, b) do { \
    uint64_t _val_a = (uint64_t)(a); \
    uint64_t _val_b = (uint64_t)(b); \
    if (_val_a == _val_b) { \
        kprintf("[KTEST][FAIL] %s: Expected %s (%p) != %s (%p) at %s:%d\n", \
                g_current_test_name, #a, (void *)_val_a, #b, (void *)_val_b, __FILE__, __LINE__); \
        g_ktest_failed++; \
        return false; \
    } \
} while (0)

#define KTEST_ASSERT_NULL(p) KTEST_ASSERT((p) == NULL)
#define KTEST_ASSERT_NOT_NULL(p) KTEST_ASSERT((p) != NULL)

#define KTEST_RUN(test_func) do { \
    g_current_test_name = #test_func; \
    kprintf("[KTEST][RUN]  %s ...\n", #test_func); \
    if (test_func()) { \
        g_ktest_passed++; \
        kprintf("[KTEST][PASS] %s\n", #test_func); \
    } \
} while (0)

// Subsystem test suite declarations
bool test_pmm_all(void);
bool test_vmm_all(void);
bool test_kstring_all(void);
bool test_tarfs_all(void);
bool test_sched_all(void);
bool test_ata_all(void);
bool test_ext2_all(void);

// Master test entry point
int ktest_run_all(void);

#endif // KERNEL_TESTS_KTEST_H
