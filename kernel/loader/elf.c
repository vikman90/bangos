#include "elf.h"
#include "mm/memory.h"
#include "drivers/uart.h"
#include <string.h>

int elf_load_binary(const void *elf_data, size_t elf_size, elf_info_t *out_info) {
    if (!elf_data || elf_size < sizeof(Elf64_Ehdr)) {
        kprintf("[ELF Error] Invalid ELF buffer pointer or size.\n");
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;

    // Check magic
    if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC) {
        kprintf("[ELF Error] Invalid ELF magic header.\n");
        return -1;
    }

    // Check 64-bit architecture
    if (ehdr->e_ident[4] != 2 || ehdr->e_machine != 0x3E) {
        kprintf("[ELF Error] Binary is not 64-bit x86_64.\n");
        return -1;
    }

    kprintf("[ELF] Parsing 64-bit ELF binary, entry point at %p\n", ehdr->e_entry);

    const uint8_t *ph_table = (const uint8_t *)elf_data + ehdr->e_phoff;

    out_info->entry_point = ehdr->e_entry;
    out_info->min_vaddr = ~0ULL;
    out_info->max_vaddr = 0;
    out_info->num_segments = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(ph_table + i * ehdr->e_phentsize);

        if (phdr->p_type == PT_LOAD) {
            uint64_t vaddr = phdr->p_vaddr;
            uint64_t memsz = phdr->p_memsz;
            uint64_t filesz = phdr->p_filesz;
            uint64_t offset = phdr->p_offset;

            kprintf("[ELF] Loading PT_LOAD segment %d: virt=%p, filesz=%u, memsz=%u\n",
                    i, vaddr, (uint32_t)filesz, (uint32_t)memsz);

            if (vaddr < out_info->min_vaddr) out_info->min_vaddr = vaddr;
            if (vaddr + memsz > out_info->max_vaddr) out_info->max_vaddr = vaddr + memsz;

            uint64_t page_aligned_vaddr = vaddr & ~0xFFFULL;
            uint64_t vaddr_offset = vaddr & 0xFFFULL;
            size_t num_pages = (memsz + vaddr_offset + PAGE_SIZE - 1) / PAGE_SIZE;

            void *phys_pages = alloc_pages(num_pages);
            if (!phys_pages) {
                kprintf("[ELF Error] Out of physical memory for PT_LOAD segment.\n");
                return -1;
            }

            // Copy file data at offset within first page
            if (filesz > 0) {
                memcpy((uint8_t *)phys_pages + vaddr_offset, (const uint8_t *)elf_data + offset, filesz);
            }
            // Zero out BSS
            if (memsz > filesz) {
                memset((uint8_t *)phys_pages + vaddr_offset + filesz, 0, memsz - filesz);
            }

            // Map segment pages into page table
            map_user_pages(page_aligned_vaddr, (uint64_t)phys_pages, num_pages);

            if (out_info->num_segments < MAX_ELF_SEGMENTS) {
                out_info->segments[out_info->num_segments].virt_addr = page_aligned_vaddr;
                out_info->segments[out_info->num_segments].phys_addr = (uint64_t)phys_pages;
                out_info->segments[out_info->num_segments].num_pages = num_pages;
                out_info->num_segments++;
            }
        }
    }

    kprintf("[ELF] All segments loaded successfully (%d segments recorded).\n", (int)out_info->num_segments);
    return 0;
}
