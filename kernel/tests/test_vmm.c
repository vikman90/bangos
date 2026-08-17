#include "ktest.h"
#include "mm/memory.h"
#include "mm/vmm.h"
#include "process/process.h"

static bool test_vmm_map_unmap_lifecycle(void) {
    uint64_t virt = 0x700000000000ULL;
    void *phys_frame = alloc_page();
    KTEST_ASSERT_NOT_NULL(phys_frame);

    map_page(virt, (uint64_t)phys_frame, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    uint64_t *pte = get_pte_ptr(virt);
    KTEST_ASSERT_NOT_NULL(pte);
    KTEST_ASSERT((*pte & PAGE_PRESENT) != 0);
    KTEST_ASSERT((*pte & PAGE_WRITABLE) != 0);
    KTEST_ASSERT_EQ(get_phys_mapping(virt), (uint64_t)phys_frame);

    // Modify flags to Read-Only
    modify_page_flags(virt, PAGE_PRESENT | PAGE_USER);
    pte = get_pte_ptr(virt);
    KTEST_ASSERT_NOT_NULL(pte);
    KTEST_ASSERT((*pte & PAGE_WRITABLE) == 0);

    // Unmap page (this internally frees phys_frame)
    unmap_page(virt);
    pte = get_pte_ptr(virt);
    KTEST_ASSERT(pte == NULL || (*pte & PAGE_PRESENT) == 0);

    return true;
}

static bool test_vma_create_and_find(void) {
    process_t mock_proc;
    vmm_init_process(&mock_proc);

    // Invalid range (start >= end) must be rejected
    vm_area_t *inv_vma = vma_create(&mock_proc, 0x800020000000ULL, 0x800010000000ULL, VMA_PROT_READ, VMA_MAP_PRIVATE);
    KTEST_ASSERT_NULL(inv_vma);

    vm_area_t *inv_vma2 = vma_create(&mock_proc, 0x800010000000ULL, 0x800010000000ULL, VMA_PROT_READ, VMA_MAP_PRIVATE);
    KTEST_ASSERT_NULL(inv_vma2);

    // Valid range
    vm_area_t *vma = vma_create(&mock_proc, 0x800010000000ULL, 0x800010040000ULL, VMA_PROT_READ | VMA_PROT_WRITE, VMA_MAP_PRIVATE);
    KTEST_ASSERT_NOT_NULL(vma);
    KTEST_ASSERT_EQ(vma->start, 0x800010000000ULL);
    KTEST_ASSERT_EQ(vma->end, 0x800010040000ULL);

    // Boundary lookups
    KTEST_ASSERT_NOT_NULL(vma_find(&mock_proc, 0x800010000000ULL));     // Start
    KTEST_ASSERT_NOT_NULL(vma_find(&mock_proc, 0x800010020000ULL));     // Middle
    KTEST_ASSERT_NOT_NULL(vma_find(&mock_proc, 0x80001003FFFFULL));     // Last byte
    KTEST_ASSERT_NULL(vma_find(&mock_proc, 0x80000FFFFFFFFULL));        // Before start
    KTEST_ASSERT_NULL(vma_find(&mock_proc, 0x800010040000ULL));         // At end bound

    return true;
}

static bool test_vma_punch_hole_split(void) {
    process_t mock_proc;
    vmm_init_process(&mock_proc);

    // Create 64 KB VMA
    vma_create(&mock_proc, 0x800010000000ULL, 0x800010020000ULL, VMA_PROT_READ | VMA_PROT_WRITE, VMA_MAP_PRIVATE);

    // Punch a hole in the middle (0x800010004000..0x800010008000)
    int res = vma_remove(&mock_proc, 0x800010004000ULL, 0x800010008000ULL);
    KTEST_ASSERT_EQ(res, 0);

    // Left slice must exist: 0x800010000000..0x800010004000
    vm_area_t *left = vma_find(&mock_proc, 0x800010002000ULL);
    KTEST_ASSERT_NOT_NULL(left);
    KTEST_ASSERT_EQ(left->start, 0x800010000000ULL);
    KTEST_ASSERT_EQ(left->end, 0x800010004000ULL);

    // Right slice must exist: 0x800010008000..0x800010020000
    vm_area_t *right = vma_find(&mock_proc, 0x800010010000ULL);
    KTEST_ASSERT_NOT_NULL(right);
    KTEST_ASSERT_EQ(right->start, 0x800010008000ULL);
    KTEST_ASSERT_EQ(right->end, 0x800010020000ULL);

    // Removed middle region must be gone
    KTEST_ASSERT_NULL(vma_find(&mock_proc, 0x800010004000ULL));
    KTEST_ASSERT_NULL(vma_find(&mock_proc, 0x800010006000ULL));
    KTEST_ASSERT_NULL(vma_find(&mock_proc, 0x800010007FFFULL));

    return true;
}

static bool test_vma_protect_split(void) {
    process_t mock_proc;
    vmm_init_process(&mock_proc);

    // Create Read-Only VMA 0x800030000000..0x800040000000
    vma_create(&mock_proc, 0x800030000000ULL, 0x800040000000ULL, VMA_PROT_READ, VMA_MAP_PRIVATE);

    // Change middle to RW
    int res = vma_protect(&mock_proc, 0x800034000000ULL, 0x800038000000ULL, VMA_PROT_READ | VMA_PROT_WRITE);
    KTEST_ASSERT_EQ(res, 0);

    vm_area_t *v1 = vma_find(&mock_proc, 0x800032000000ULL);
    KTEST_ASSERT_NOT_NULL(v1);
    KTEST_ASSERT_EQ(v1->prot, VMA_PROT_READ);

    vm_area_t *v2 = vma_find(&mock_proc, 0x800036000000ULL);
    KTEST_ASSERT_NOT_NULL(v2);
    KTEST_ASSERT_EQ(v2->prot, VMA_PROT_READ | VMA_PROT_WRITE);

    vm_area_t *v3 = vma_find(&mock_proc, 0x80003A000000ULL);
    KTEST_ASSERT_NOT_NULL(v3);
    KTEST_ASSERT_EQ(v3->prot, VMA_PROT_READ);

    return true;
}

bool test_vmm_all(void) {
    kprintf("\n--- [KTEST] Running Virtual Memory Manager (VMM & VMA) Tests ---\n");
    KTEST_RUN(test_vmm_map_unmap_lifecycle);
    KTEST_RUN(test_vma_create_and_find);
    KTEST_RUN(test_vma_punch_hole_split);
    KTEST_RUN(test_vma_protect_split);
    return true;
}
