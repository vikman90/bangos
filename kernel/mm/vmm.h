#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch/x86_64/idt.h"

// Forward declaration of struct process
struct process;

#define MAX_PROCESS_VMAS 32

// VMA Protection Flags
#define VMA_PROT_NONE   0x0
#define VMA_PROT_READ   0x1
#define VMA_PROT_WRITE  0x2
#define VMA_PROT_EXEC   0x4

// VMA Mapping Flags
#define VMA_MAP_SHARED    0x01
#define VMA_MAP_PRIVATE   0x02
#define VMA_MAP_FIXED     0x10
#define VMA_MAP_ANONYMOUS 0x20

#define MMAP_BASE_START   0x700000000000ULL

typedef struct vm_area {
    uint64_t start;  // Page-aligned virtual start address
    uint64_t end;    // Page-aligned virtual end address (start + length)
    uint32_t prot;   // VMA_PROT_*
    uint32_t flags;  // VMA_MAP_*
    bool     used;
} vm_area_t;

void       vmm_init_process(struct process *proc);
vm_area_t *vma_create(struct process *proc, uint64_t start, uint64_t end, uint32_t prot, uint32_t flags);
vm_area_t *vma_find(struct process *proc, uint64_t addr);
int        vma_remove(struct process *proc, uint64_t start, uint64_t end);
int        vma_protect(struct process *proc, uint64_t start, uint64_t end, uint32_t new_prot);
uint64_t   vmm_find_free_area(struct process *proc, size_t length);
int        vmm_handle_page_fault(context_frame_t *frame);

#endif /* VMM_H */
