#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    void     *memory_map;
    uint64_t  memory_map_size;
    uint64_t  descriptor_size;
    uint32_t  descriptor_version;
    void     *elf_paddr;
    uint64_t  elf_size;
} boot_info_t;

void kernel_main(boot_info_t *boot_info);

#endif /* KERNEL_H */
