#include "ktest.h"
#include "mm/memory.h"
#include "lib/kstring.h"

static bool test_pmm_zero_alloc(void) {
    void *ptr = alloc_pages(0);
    KTEST_ASSERT_NULL(ptr);
    return true;
}

static bool test_pmm_single_page_lifecycle(void) {
    size_t free_before = mm_get_free_bytes();
    void *p = alloc_page();
    KTEST_ASSERT_NOT_NULL(p);
    KTEST_ASSERT_EQ((uint64_t)p & 0xFFF, 0); // 4096-byte aligned

    // Verify zeroed page
    uint8_t *byte_ptr = (uint8_t *)p;
    bool all_zero = true;
    for (int i = 0; i < 4096; i++) {
        if (byte_ptr[i] != 0) { all_zero = false; break; }
    }
    KTEST_ASSERT(all_zero);

    // Write signature
    byte_ptr[0] = 0xAA;
    byte_ptr[4095] = 0x55;

    size_t free_after_alloc = mm_get_free_bytes();
    KTEST_ASSERT_EQ(free_before - free_after_alloc, PAGE_SIZE);

    free_page(p);
    size_t free_after_free = mm_get_free_bytes();
    KTEST_ASSERT_EQ(free_after_free, free_before);

    return true;
}

static bool test_pmm_multi_page_contiguous(void) {
    const size_t COUNT = 4;
    size_t free_before = mm_get_free_bytes();

    void *p = alloc_pages(COUNT);
    KTEST_ASSERT_NOT_NULL(p);
    KTEST_ASSERT_EQ((uint64_t)p & 0xFFF, 0);

    size_t free_after_alloc = mm_get_free_bytes();
    KTEST_ASSERT_EQ(free_before - free_after_alloc, COUNT * PAGE_SIZE);

    // Free individual frames
    uint8_t *curr = (uint8_t *)p;
    for (size_t i = 0; i < COUNT; i++) {
        free_page(curr + (i * PAGE_SIZE));
    }

    size_t free_after_free = mm_get_free_bytes();
    KTEST_ASSERT_EQ(free_after_free, free_before);

    return true;
}

static bool test_pmm_free_safety(void) {
    size_t free_before = mm_get_free_bytes();

    // Freeing NULL should be a safe no-op
    free_page(NULL);
    KTEST_ASSERT_EQ(mm_get_free_bytes(), free_before);

    // Freeing address below physical pool base should be a safe no-op
    free_page((void *)0x1000ULL);
    free_page((void *)0x100000ULL);
    KTEST_ASSERT_EQ(mm_get_free_bytes(), free_before);

    return true;
}

static bool test_pmm_memory_bounds(void) {
    size_t total = mm_get_total_bytes();
    size_t free = mm_get_free_bytes();

    KTEST_ASSERT(total > 0);
    KTEST_ASSERT(free > 0);
    KTEST_ASSERT(free <= total);

    return true;
}

bool test_pmm_all(void) {
    kprintf("\n--- [KTEST] Running Physical Memory Manager (PMM) Tests ---\n");
    KTEST_RUN(test_pmm_zero_alloc);
    KTEST_RUN(test_pmm_single_page_lifecycle);
    KTEST_RUN(test_pmm_multi_page_contiguous);
    KTEST_RUN(test_pmm_free_safety);
    KTEST_RUN(test_pmm_memory_bounds);
    return true;
}
