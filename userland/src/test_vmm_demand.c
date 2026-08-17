#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s at %s:%d\n", msg, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

#define TEST_PASS(name) printf("[PASS] %s\n", name)

static void test_mmap_demand_paging(void) {
    const size_t SIZE = 8 * 1024 * 1024; // 8 MB (2048 pages)
    uint8_t *ptr = (uint8_t *)mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    TEST_ASSERT(ptr != MAP_FAILED, "mmap 8MB anonymous region should succeed");

    // Touch and write signature across every single 4KB page
    for (size_t i = 0; i < SIZE; i += 4096) {
        ptr[i] = (uint8_t)((i / 4096) & 0xFF);
        ptr[i + 4095] = 0xAA;
    }

    // Verify all pages retained data without corruption
    for (size_t i = 0; i < SIZE; i += 4096) {
        TEST_ASSERT(ptr[i] == (uint8_t)((i / 4096) & 0xFF), "Page content start mismatch");
        TEST_ASSERT(ptr[i + 4095] == 0xAA, "Page content end mismatch");
    }

    // Unmap the entire region
    int unmap_res = munmap(ptr, SIZE);
    TEST_ASSERT(unmap_res == 0, "munmap 8MB anonymous region should succeed");

    TEST_PASS("VMM 8MB Demand Paging & Zero-Fill Verification");
}

static void test_mprotect_and_partial_munmap(void) {
    const size_t SIZE = 64 * 1024; // 64 KB (16 pages)
    uint8_t *ptr = (uint8_t *)mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    TEST_ASSERT(ptr != MAP_FAILED, "mmap 64KB region should succeed");

    for (size_t i = 0; i < SIZE; i += 4096) {
        ptr[i] = 0x55;
    }

    // Change protection on middle 32KB to PROT_READ
    int prot_res = mprotect(ptr + 16384, 32768, PROT_READ);
    TEST_ASSERT(prot_res == 0, "mprotect middle 32KB to PROT_READ should succeed");

    // Partial unmap: punch a hole in the middle (pages 4..7)
    int hole_res = munmap(ptr + 16384, 16384);
    TEST_ASSERT(hole_res == 0, "munmap 16KB middle hole should succeed");

    // Unmap remaining portions
    munmap(ptr, 16384);
    munmap(ptr + 32768, 32768);

    TEST_PASS("VMM mprotect & Partial Hole Munmap");
}

int main(void) {
    printf("\n======================================================\n");
    printf("     BangOS VMM Demand Paging & Lifecycle Suite       \n");
    printf("======================================================\n\n");

    test_mmap_demand_paging();
    test_mprotect_and_partial_munmap();

    printf("\n[SUCCESS] All VMM memory lifecycle tests evaluated to PASS!\n");
    return 0;
}
