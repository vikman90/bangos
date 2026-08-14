#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "../include/kernel.h"

#define PAGE_SIZE 4096

void  mm_init(boot_info_t *boot_info);
void *alloc_page(void);
void  free_page(void *ptr);
void *alloc_pages(size_t count);

// Page Table Flags
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

void map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void map_user_pages(uint64_t virt, uint64_t phys, size_t page_count);

size_t mm_get_total_bytes(void);
size_t mm_get_free_bytes(void);

#endif /* MEMORY_H */
