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
    union {
        void *ramdisk_paddr;
        void *elf_paddr;
    };
    union {
        uint64_t ramdisk_size;
        uint64_t elf_size;
    };
} boot_info_t;

void kernel_main(boot_info_t *boot_info);

#endif /* KERNEL_H */
