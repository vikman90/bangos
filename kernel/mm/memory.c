#include "memory.h"
#include "drivers/uart.h"
#include <string.h>

#define MAX_PAGES 32768 // 128 MB physical pool
static uint8_t page_bitmap[MAX_PAGES / 8];
static uint64_t phys_memory_base = 0x2000000ULL; // 32 MB start for dynamic allocations

static uint64_t *kernel_pml4 = NULL;

static inline void write_cr3(uint64_t cr3) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline void invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void *alloc_pages(size_t count) {
    if (count == 0) return NULL;

    for (size_t i = 0; i <= MAX_PAGES - count; ) {
        // Fast-path: skip fully allocated 64-frame blocks
        if ((i % 64 == 0) && count <= 64) {
            const uint64_t *bm64 = (const uint64_t *)page_bitmap;
            while (i <= MAX_PAGES - 64 && bm64[i / 64] == ~0ULL) {
                i += 64;
            }
            if (i > MAX_PAGES - count) break;
        }

        bool free_found = true;
        size_t next_check = i + 1;
        for (size_t j = 0; j < count; j++) {
            if (page_bitmap[(i + j) / 8] & (1 << ((i + j) % 8))) {
                free_found = false;
                next_check = i + j + 1;
                break;
            }
        }

        if (free_found) {
            for (size_t j = 0; j < count; j++) {
                page_bitmap[(i + j) / 8] |= (1 << ((i + j) % 8));
            }
            uint64_t addr = phys_memory_base + (i * PAGE_SIZE);
            memset((void *)addr, 0, count * PAGE_SIZE);
            return (void *)addr;
        }

        i = next_check;
    }
    kprintf("[MM Panic] Out of physical memory frames!\n");
    return NULL;
}

void free_page(void *ptr) {
    uint64_t addr = (uint64_t)ptr;
    if (addr < phys_memory_base) return;
    size_t index = (addr - phys_memory_base) / PAGE_SIZE;
    if (index < MAX_PAGES) {
        page_bitmap[index / 8] &= ~(1 << (index % 8));
    }
}

void *alloc_page(void) {
    return alloc_pages(1);
}

void mm_init(boot_info_t *boot_info) {
    (void)boot_info;
    memset(page_bitmap, 0, sizeof(page_bitmap));

    // Allocate custom kernel PML4 page table
    kernel_pml4 = (uint64_t *)alloc_page();
    uint64_t *pdpt = (uint64_t *)alloc_page();
    kernel_pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    // Identity map 0GB..4GB using 2MB huge pages across 4 Page Directories
    for (size_t gb = 0; gb < 4; gb++) {
        uint64_t *pd = (uint64_t *)alloc_page();
        pdpt[gb] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

        for (size_t entry = 0; entry < 512; entry++) {
            uint64_t phys_addr = (gb * 1024 * 1024 * 1024ULL) + (entry * 0x200000ULL);
            pd[entry] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | (1ULL << 7);
        }
    }

    // Switch to kernel PML4 page table
    write_cr3((uint64_t)kernel_pml4);

    kprintf("[MM] Dedicated 64-bit Paging CR3 loaded (%p), 4GB identity mapped.\n", kernel_pml4);
}

void map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) {
        void *new_pdpt = alloc_page();
        kernel_pml4[pml4_idx] = (uint64_t)new_pdpt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    uint64_t *pdpt = (uint64_t *)(kernel_pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        void *new_pd = alloc_page();
        pdpt[pdpt_idx] = (uint64_t)new_pd | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ~0xFFFULL);

    if (pd[pd_idx] & (1ULL << 7)) {
        uint64_t huge_phys = pd[pd_idx] & ~0x1FFFFFULL;
        void *new_pt = alloc_page();
        uint64_t *pt = (uint64_t *)new_pt;
        for (size_t i = 0; i < 512; i++) {
            pt[i] = (huge_phys + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        }
        pd[pd_idx] = (uint64_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    } else if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void *new_pt = alloc_page();
        pd[pd_idx] = (uint64_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = (phys & ~0xFFFULL) | flags;

    invlpg(virt);
}

void map_user_pages(uint64_t virt, uint64_t phys, size_t page_count) {
    for (size_t i = 0; i < page_count; i++) {
        map_page(virt + (i * PAGE_SIZE), phys + (i * PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }
}

size_t mm_get_total_bytes(void) {
    return (size_t)MAX_PAGES * PAGE_SIZE;
}

size_t mm_get_free_bytes(void) {
    size_t free_count = 0;
    for (size_t i = 0; i < MAX_PAGES; i++) {
        if ((page_bitmap[i / 8] & (1 << (i % 8))) == 0) {
            free_count++;
        }
    }
    return free_count * PAGE_SIZE;
}
