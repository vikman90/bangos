# 03 - x86_64 Paging and Memory Management

In x86_64, paging is mandatory to enable Long Mode. BangOS implements a 4-level paging system using hardware-backed x86_64 page table hierarchies.

---

## 🗺️ 4-Level Page Table Structure

```
      CR3 Register (PML4 Pointer)
                 |
                 v
   PML4 (Page Map Level 4)    [512 entries x 64 bits]
                 |
                 v
   PDPT (Page Directory Pointer Table)
                 |
                 v
   PD   (Page Directory Table) [2MB Huge Pages or PT pointers]
                 |
                 v
   PT   (Page Table)           [4KB Individual Pages]
```

---

## 🔒 4GB Kernel Identity Mapping (`kernel/mm/memory.c`)

During `mm_init()`, BangOS constructs a primary page table:
- Identity maps (Virtual Address == Physical Address) the first **4 GB of physical RAM** (`0x00000000` .. `0xFFFFFFFF`) using 2MB huge pages (`PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER`).
- Loads the PML4 physical address into control register `%cr3`:
  ```c
  write_cr3((uint64_t)kernel_pml4);
  ```

---

## 🛠️ 4KB Physical Frame Allocator

BangOS implements a bitmap-based physical frame allocator:
- Manages a 128 MB physical memory pool (`32,768` 4KB frames).
- `alloc_page()`: Allocates a zero-initialized 4KB physical frame.
- `alloc_pages(count)`: Allocates $N$ contiguous physical frames.
- `free_page(ptr)`: Releases a frame and updates the bitmap.

---

## 🌐 Dynamic User Page Mapper

The `map_user_pages(virt, phys, count)` function maps any virtual address range to physical memory:
1. Traverses or creates PML4, PDPT, PD, and PT levels on demand.
2. Dynamically splits 2MB huge pages into 512 4KB entries if required.
3. Invalidates the TLB entry using `invlpg` and reloads `%cr3` to synchronize the CPU cache.
