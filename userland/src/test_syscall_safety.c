#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <string.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s at %s:%d\n", msg, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

#define TEST_PASS(name) printf("[PASS] %s\n", name)

static void test_bad_file_descriptors(void) {
    char buf[16];
    ssize_t res;

    // Bad read fd
    res = syscall(SYS_read, -1, buf, sizeof(buf));
    TEST_ASSERT(res == -1 && errno == EBADF, "SYS_read on fd=-1 must return -1 with EBADF");

    res = syscall(SYS_read, 999, buf, sizeof(buf));
    TEST_ASSERT(res == -1 && errno == EBADF, "SYS_read on fd=999 must return -1 with EBADF");

    // Bad write fd
    res = syscall(SYS_write, -1, "test", 4);
    TEST_ASSERT(res == -1 && errno == EBADF, "SYS_write on fd=-1 must return -1 with EBADF");

    res = syscall(SYS_write, 100, "test", 4);
    TEST_ASSERT(res == -1 && errno == EBADF, "SYS_write on fd=100 must return -1 with EBADF");

    TEST_PASS("Bad File Descriptor Error Propagation (-EBADF)");
}

static void test_null_and_kernel_pointers(void) {
    ssize_t res;

    // NULL buffer in write
    res = syscall(SYS_write, 1, NULL, 10);
    TEST_ASSERT(res == -1 && errno == EFAULT, "SYS_write with NULL buffer must return -1 with EFAULT");

    // NULL in uname
    res = syscall(SYS_uname, NULL);
    TEST_ASSERT(res == -1 && errno == EFAULT, "SYS_uname with NULL buffer must return -1 with EFAULT");

    // NULL in sysinfo
    res = syscall(SYS_sysinfo, NULL);
    TEST_ASSERT(res == -1 && errno == EFAULT, "SYS_sysinfo with NULL buffer must return -1 with EFAULT");

    // NULL in clock_gettime
    res = syscall(SYS_clock_gettime, 0, NULL);
    TEST_ASSERT(res == -1 && errno == EFAULT, "SYS_clock_gettime with NULL buffer must return -1 with EFAULT");

    TEST_PASS("Null & Kernel Pointer Safety Checking (-EFAULT)");
}

static void test_unimplemented_syscalls(void) {
    // Unimplemented syscall number (e.g. 999) must return -1 with ENOSYS
    long res = syscall(999);
    TEST_ASSERT(res == -1 && errno == ENOSYS, "Unimplemented syscall #999 must return -1 with ENOSYS");

    res = syscall(888, 1, 2, 3);
    TEST_ASSERT(res == -1 && errno == ENOSYS, "Unimplemented syscall #888 must return -1 with ENOSYS");

    TEST_PASS("Unimplemented System Call Dispatch Invariant (-ENOSYS)");
}

static void test_zero_length_operations(void) {
    char buf[16] = "hello";

    // Write with count 0
    ssize_t res = syscall(SYS_write, 1, buf, 0);
    TEST_ASSERT(res == 0, "SYS_write with count=0 must return 0 bytes written");

    TEST_PASS("Zero-Length I/O Operations");
}

int main(void) {
    printf("\n======================================================\n");
    printf("   BangOS Syscall Safety & POSIX Contract Test Suite  \n");
    printf("======================================================\n\n");

    test_bad_file_descriptors();
    test_null_and_kernel_pointers();
    test_unimplemented_syscalls();
    test_zero_length_operations();

    printf("\n[SUCCESS] All syscall safety contracts evaluated to PASS!\n");
    return 0;
}
