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

static inline void write_msr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

#endif /* KERNEL_H */
