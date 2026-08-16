#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

#define ELF_MAGIC 0x464C457F // "\x7fELF"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

#define MAX_ELF_SEGMENTS 8

typedef struct {
    uint64_t virt_addr;
    uint64_t phys_addr;
    size_t   num_pages;
} elf_segment_t;

typedef struct {
    uint64_t      entry_point;
    uint64_t      min_vaddr;
    uint64_t      max_vaddr;
    size_t        num_segments;
    elf_segment_t segments[MAX_ELF_SEGMENTS];
} elf_info_t;

int elf_load_binary(const void *elf_data, size_t elf_size, elf_info_t *out_info);

#endif /* ELF_H */
