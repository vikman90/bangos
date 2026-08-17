#include "ktest.h"
#include "process/process.h"

static bool test_process_table_basics(void) {
    process_t *curr = process_get_current();
    KTEST_ASSERT_NOT_NULL(curr);
    return true;
}

static bool test_futex_mismatch_and_args(void) {
    uint32_t val = 1234;

    // NULL address returns -EFAULT (-14)
    KTEST_ASSERT_EQ(futex_wait(NULL, 1234), -14);
    KTEST_ASSERT_EQ(futex_wake(NULL, 1), 0);
    KTEST_ASSERT_EQ(futex_wake(&val, -1), 0);

    // Value mismatch (*uaddr != val) must immediately return -EAGAIN (-11)
    KTEST_ASSERT_EQ(futex_wait(&val, 9999), -11);

    return true;
}

bool test_sched_all(void) {
    kprintf("\n--- [KTEST] Running Process & Synchronization Primitive Tests ---\n");
    KTEST_RUN(test_process_table_basics);
    KTEST_RUN(test_futex_mismatch_and_args);
    return true;
}
