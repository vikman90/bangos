#include "vmm.h"
#include "memory.h"
#include "process/process.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

void vmm_init_process(struct process *proc) {
    if (!proc) return;
    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        proc->vmas[i].used = false;
        proc->vmas[i].start = 0;
        proc->vmas[i].end = 0;
        proc->vmas[i].prot = 0;
        proc->vmas[i].flags = 0;
    }
    proc->mmap_curr_base = MMAP_BASE_START + ((uint64_t)proc->pid * 0x100000000ULL);
}

vm_area_t *vma_create(struct process *proc, uint64_t start, uint64_t end, uint32_t prot, uint32_t flags) {

    if (!proc || start >= end) return NULL;

    // 1. Check if already inside or adjacent to an existing matching VMA
    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        if (proc->vmas[i].used && proc->vmas[i].prot == prot && proc->vmas[i].flags == flags) {
            if (start >= proc->vmas[i].start && end <= proc->vmas[i].end) {
                return &proc->vmas[i];
            }
            if (proc->vmas[i].end == start) {
                proc->vmas[i].end = end;
                return &proc->vmas[i];
            }
            if (proc->vmas[i].start == end) {
                proc->vmas[i].start = start;
                return &proc->vmas[i];
            }
        }
    }

    // 2. Find a free slot
    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        if (!proc->vmas[i].used) {
            proc->vmas[i].start = start;
            proc->vmas[i].end = end;
            proc->vmas[i].prot = prot;
            proc->vmas[i].flags = flags;
            proc->vmas[i].used = true;
            return &proc->vmas[i];
        }
    }

    kprintf("[VMM Warning] Process PID=%d reached maximum VMAs (%d)!\n", proc->pid, MAX_PROCESS_VMAS);
    return NULL;
}


vm_area_t *vma_find(struct process *proc, uint64_t addr) {
    if (!proc) return NULL;

    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        if (proc->vmas[i].used && addr >= proc->vmas[i].start && addr < proc->vmas[i].end) {
            return &proc->vmas[i];
        }
    }
    return NULL;
}

int vma_remove(struct process *proc, uint64_t start, uint64_t end) {
    if (!proc || start >= end) return -22; // -EINVAL

    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        if (!proc->vmas[i].used) continue;

        vm_area_t *v = &proc->vmas[i];
        // Check for overlap
        if (start < v->end && end > v->start) {
            uint64_t unmap_start = (start > v->start) ? start : v->start;
            uint64_t unmap_end   = (end < v->end) ? end : v->end;

            size_t page_count = (unmap_end - unmap_start) / PAGE_SIZE;
            unmap_user_pages(unmap_start, page_count);

            if (start <= v->start && end >= v->end) {
                // Entire VMA removed
                v->used = false;
            } else if (start <= v->start && end < v->end) {
                // Trim from start
                v->start = end;
            } else if (start > v->start && end >= v->end) {
                // Trim from end
                v->end = start;
            } else {
                // Split VMA: shrink current to start, create new for remainder
                uint64_t orig_end = v->end;
                v->end = start;
                vma_create(proc, end, orig_end, v->prot, v->flags);
            }
        }
    }
    return 0;
}

int vma_protect(struct process *proc, uint64_t start, uint64_t end, uint32_t new_prot) {
    if (!proc || start >= end) return -22; // -EINVAL

    for (int i = 0; i < MAX_PROCESS_VMAS; i++) {
        if (!proc->vmas[i].used) continue;

        vm_area_t *v = &proc->vmas[i];
        if (start < v->end && end > v->start) {
            uint64_t prot_start = (start > v->start) ? start : v->start;
            uint64_t prot_end   = (end < v->end) ? end : v->end;

            if (start <= v->start && end >= v->end) {
                v->prot = new_prot;
            } else if (start <= v->start && end < v->end) {
                uint64_t old_end = v->end;
                uint32_t old_prot = v->prot;
                uint32_t old_flags = v->flags;
                v->end = end;
                v->prot = new_prot;
                vma_create(proc, end, old_end, old_prot, old_flags);
            } else if (start > v->start && end >= v->end) {
                uint32_t old_flags = v->flags;
                v->end = start;
                vma_create(proc, start, end, new_prot, old_flags);
            } else {
                uint64_t old_end = v->end;
                uint32_t old_prot = v->prot;
                uint32_t old_flags = v->flags;
                v->end = start;
                vma_create(proc, start, end, new_prot, old_flags);
                vma_create(proc, end, old_end, old_prot, old_flags);
            }

            // Update flags for any pages in this range that have already been faulted in
            uint64_t pte_flags = PAGE_PRESENT | PAGE_USER;
            if (new_prot & VMA_PROT_WRITE) pte_flags |= PAGE_WRITABLE;

            for (uint64_t addr = prot_start; addr < prot_end; addr += PAGE_SIZE) {
                uint64_t *pte = get_pte_ptr(addr);
                if (pte && (*pte & PAGE_PRESENT)) {
                    modify_page_flags(addr, pte_flags);
                }
            }
        }
    }
    return 0;
}


uint64_t vmm_find_free_area(struct process *proc, size_t length) {
    if (!proc) return 0;

    size_t aligned_len = (length + PAGE_SIZE - 1) & ~0xFFFULL;
    uint64_t candidate = (proc->mmap_curr_base + PAGE_SIZE - 1) & ~0xFFFULL;

    // Advance candidate base
    proc->mmap_curr_base = candidate + aligned_len;
    return candidate;
}

int vmm_handle_page_fault(context_frame_t *frame) {
    uint64_t fault_addr = read_cr2();
    uint64_t error_code = frame->error_code;
    process_t *proc = process_get_current();

    if (!proc || !proc->active) {
        kprintf("[Kernel Panic] Page fault in kernel context or non-active process! CR2=%p RIP=%p\n",
                fault_addr, frame->rip);
        kprintf("[VMM #PF Error] No active process for fault_addr=%p RIP=%p\n", fault_addr, frame->rip);
        return -1;
    }

    vm_area_t *vma = vma_find(proc, fault_addr);
    if (!vma) {
        kprintf("[VMM #PF Error] No VMA found for PID=%d fault_addr=%p RIP=%p\n",
                proc->pid, fault_addr, frame->rip);
        return -1;
    }

    // Check for protection violation (P bit = 1 in error code)
    if (error_code & 1) {
        if ((error_code & 2) && !(vma->prot & VMA_PROT_WRITE)) {
            kprintf("[VMM #PF Error] Write protection violation for PID=%d at %p (VMA prot=%x)\n",
                    proc->pid, fault_addr, vma->prot);
            return -1;
        }
    }

    // Demand page allocation for non-present page
    uint64_t page_addr = fault_addr & ~0xFFFULL;
    void *phys_frame = alloc_page();
    if (!phys_frame) {
        kprintf("[VMM Error] Out of physical memory handling demand page fault at %p for PID=%d\n",
                page_addr, proc->pid);
        return -1;
    }

    // Zero-fill anonymous memory
    if (vma->flags & VMA_MAP_ANONYMOUS) {
        kmemset(phys_frame, 0, PAGE_SIZE);
    }

    uint64_t pte_flags = PAGE_PRESENT | PAGE_USER;
    if (vma->prot & VMA_PROT_WRITE) {
        pte_flags |= PAGE_WRITABLE;
    }

    map_page(page_addr, (uint64_t)phys_frame, pte_flags);

    return 0;
}
