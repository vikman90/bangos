#include "ktest.h"
#include "drivers/uart.h"
#include "drivers/qemu.h"

__attribute__((visibility("hidden"))) int g_ktest_passed = 0;
__attribute__((visibility("hidden"))) int g_ktest_failed = 0;
__attribute__((visibility("hidden"))) const char *g_current_test_name = "";

int ktest_run_all(void) {
    g_ktest_passed = 0;
    g_ktest_failed = 0;

    kprintf("\n======================================================\n");
    kprintf("       BangOS In-Kernel Unit Test Suite (Ring 0)      \n");
    kprintf("======================================================\n");

    test_kstring_all();
    test_pmm_all();
    test_vmm_all();
    test_tarfs_all();
    test_sched_all();

    kprintf("\n------------------------------------------------------\n");
    kprintf("[KTEST SUMMARY] Total Passed: %d | Total Failed: %d\n",
            g_ktest_passed, g_ktest_failed);
    kprintf("======================================================\n");

    if (g_ktest_failed == 0) {
        kprintf("[KTEST SUCCESS] All Ring 0 kernel self-tests evaluated to PASS!\n\n");
    } else {
        kprintf("[KTEST FAILURE] Detected %d failed assertions in Ring 0 tests!\n\n", g_ktest_failed);
    }

    return g_ktest_failed;
}
