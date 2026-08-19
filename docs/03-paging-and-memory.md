# 03 - 4-Level Paging, Frame Allocation & Virtual Memory Manager

In the x86_64 architecture, paging is mandatory in Long Mode. BangOS implements a complete virtual memory subsystem comprising:

1. **4-Level Paging Hierarchy** (PML4 -> PDPT -> PD -> PT) with a 4 GB identity-mapped kernel region.
2. **Bitmap-Based Physical Memory Manager (PMM)** allocating 4 KB frames.
3. **Dynamic Page Mapper & 2MB Huge-Page Splitting**.
4. **Virtual Memory Area (VMA) Subsystem** tracking per-process memory mappings.
5. **Demand Paging Engine** handling Page Faults (`#PF`, Vector 14) dynamically.

---

## 🗺️ 4-Level Virtual Address Translation Math

A 64-bit canonical virtual address is translated by the Memory Management Unit (MMU) through four table levels:

```text
64-Bit Canonical Virtual Address:
 63          48 47     39 38     30 29     21 20     12 11          0
+--------------+---------+---------+---------+---------+-------------+
| Sign Extend  | PML4    | PDPT    | PD      | PT      | Page Offset |
|   16 Bits    | 9 Bits  | 9 Bits  | 9 Bits  | 9 Bits  |   12 Bits   |
+--------------+---------+---------+---------+---------+-------------+
      |             |         |         |         |            |
      v             v         v         v         v            v
 Canonical     CR3->PML4    PDPT        PD        PT      Physical Byte
 Validation    [512 ent]  [512 ent]  [512 ent] [512 ent]   Offset (4KB)
```

### Table Index Extraction:
```c
size_t pml4_idx = (virt >> 39) & 0x1FF; // Bits 39..47
size_t pdpt_idx = (virt >> 30) & 0x1FF; // Bits 30..38
size_t pd_idx   = (virt >> 21) & 0x1FF; // Bits 21..29
size_t pt_idx   = (virt >> 12) & 0x1FF; // Bits 12..20
```

### Hardware Page Table Entry (PTE) Flags:

- **`PAGE_PRESENT` (`1 << 0`)**: Page is present in physical memory.
- **`PAGE_WRITABLE` (`1 << 1`)**: Read/write access allowed (if cleared, read-only).
- **`PAGE_USER` (`1 << 2`)**: Userland (Ring 3) access allowed (if cleared, supervisor only).
- **`PAGE_HUGE` (`1 << 7`)**: In Page Directory (PD), designates a 2MB huge page rather than a pointer to a Page Table (PT).

---

## 🔒 4GB Kernel Identity Mapping (`kernel/mm/memory.c`)

During early boot (`mm_init()`), BangOS allocates a dedicated PML4 and constructs a permanent identity mapping:

- **Range**: `0x0000000000000000` to `0x00000000FFFFFFFF` (First 4 GB of RAM).
- **Implementation**: 4 Page Directory entries in PDPT, each populated with 512 2MB huge pages (`PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | (1ULL << 7)`).
- **CR3 Load**: The physical address of `kernel_pml4` is loaded into CPU control register `%cr3`:
  ```c
  write_cr3((uint64_t)kernel_pml4);
  ```

This identity mapping ensures that the kernel, UEFI structures, ACPI tables, video framebuffers, and physical frame pointers can be accessed directly without translation overhead.

---

## 🛠️ Bitmap Physical Memory Manager (PMM)

BangOS manages physical RAM using a bitmap allocator:

- **Physical Pool**: 128 MB managed pool (`MAX_PAGES = 32,768` 4KB pages) starting at physical base `0x2000000` (32 MB).
- **Allocation Operations**:
  - `alloc_page()`: Finds a free bit in the bitmap, marks it used, zero-initializes the 4096-byte frame, and returns its physical address.
  - `alloc_pages(count)`: Allocates $N$ physically contiguous frames using 64-bit word fast-skipping.
  - `free_page(ptr)`: Calculates the frame index and clears the corresponding bitmap bit.
  - `mm_get_total_bytes()` & `mm_get_free_bytes()`: Query memory statistics for the `sysinfo` system call.

---

## 🌐 Dynamic User Page Mapper & 2MB Huge-Page Splitting

When user applications request arbitrary 4KB virtual addresses (for ELF code, stack, or heap), `map_page()` dynamically traverses the page table hierarchy:

1. Allocates missing PDPT, PD, or PT levels on-demand.
2. **Huge-Page Splitting**: If a 2MB huge page occupies the target Page Directory slot, `map_page()` transparently allocates a new 4KB Page Table (PT), maps 512 individual 4KB frames covering the 2MB region to preserve existing memory contents, and replaces the 2MB huge entry.
3. Updates the target PTE with the requested flags (`PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE`).
4. Flushes the Translation Lookaside Buffer (TLB) via `invlpg` or reloads `%cr3`.

---

## 📦 Virtual Memory Areas (VMA) Subsystem (`kernel/mm/vmm.c`)

Each user process maintains an array of Virtual Memory Areas (`proc->vmas`) describing allocated virtual memory ranges:

```c
typedef struct vm_area {
    uint64_t start;
    uint64_t end;
    uint32_t prot;   // VMA_PROT_READ | VMA_PROT_WRITE | VMA_PROT_EXEC
    uint32_t flags;  // VMA_MAP_PRIVATE | VMA_MAP_SHARED | VMA_MAP_ANONYMOUS
    bool     used;
} vm_area_t;
```

### VMA Subsystem Functions:

- `vmm_init_process(proc)`: Initializes process VMAs and assigns a unique 4GB per-process virtual address space base (`0x4000000000 + PID * 4GB`).
- `vma_create(proc, start, end, prot, flags)`: Registers a new virtual region, automatically coalescing adjacent contiguous regions with identical permissions.
- `vma_find(proc, addr)`: Finds the VMA enclosing a virtual address.
- `vma_protect(proc, start, end, new_prot)`: Handles `mprotect()`, modifying permissions, updating hardware PTEs, and splitting VMAs if needed.
- `vma_remove(proc, start, end)`: Handles `munmap()`, unmapping pages, reclaiming physical frames, and trimming/splitting VMAs ("punching holes").

---

## ⚡ Demand Paging on Page Fault (`#PF`, Vector 14)

Demand Paging allows processes to allocate large virtual memory regions (e.g. `mmap(8MB)`) instantly without allocating physical RAM upfront. Physical frames are allocated lazily as pages are accessed.

```text
Userland accesses unmapped virtual address
                   |
                   v
+--------------------------------------------------------------+
| CPU triggers Page Fault Exception (#PF, Vector 14)           |
| - Hardware loads faulting address into CR2                   |
| - Pushes error code (P=0 for non-present page)               |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| Kernel exception_handler() -> vmm_handle_page_fault()        |
+--------------------------------------------------------------+
                               |
            +------------------+------------------+
            |                                     |
       VMA Exists?                           No VMA Found
            |                                     |
            v                                     v
+--------------------------+          +--------------------------+
| Check Permissions        |          | Segmentation Fault       |
| (Read/Write Violation?)  |          | Send SIGSEGV / Terminate |
+--------------------------+          +--------------------------+
            | (Valid)
            v
+--------------------------------------------------------------+
| 1. alloc_page() allocates 4KB physical frame                 |
| 2. If VMA_MAP_ANONYMOUS: zero-fills 4096 bytes               |
| 3. map_page() maps virtual address -> physical frame in PT   |
| 4. Return 0 to iretq -> CPU retries instruction seamlessly   |
+--------------------------------------------------------------+
```

### Page Fault Resolution Implementation:
```c
int vmm_handle_page_fault(context_frame_t *frame) {
    uint64_t fault_addr = read_cr2();
    process_t *proc = process_get_current();

    if (!proc || !proc->active) return -1;

    vm_area_t *vma = vma_find(proc, fault_addr);
    if (!vma) return -1; // Invalid memory access -> SIGSEGV

    // Allocate physical frame on-demand
    uint64_t page_addr = fault_addr & ~0xFFFULL;
    void *phys_frame = alloc_page();
    if (!phys_frame) return -1; // Out of memory

    // Zero-fill anonymous memory pages
    if (vma->flags & VMA_MAP_ANONYMOUS) {
        kmemset(phys_frame, 0, PAGE_SIZE);
    }

    uint64_t pte_flags = PAGE_PRESENT | PAGE_USER;
    if (vma->prot & VMA_PROT_WRITE) {
        pte_flags |= PAGE_WRITABLE;
    }

    map_page(page_addr, (uint64_t)phys_frame, pte_flags);
    return 0; // Successfully resolved!
}
```
