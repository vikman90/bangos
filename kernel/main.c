#include "kernel.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "syscall/syscall.h"
#include "drivers/uart.h"
#include "mm/memory.h"
#include "loader/elf.h"
#include "process/process.h"
#include "fs/tarfs.h"
#include "fs/vfs.h"
#include "drivers/block.h"
#include "drivers/ata.h"
#include "fs/ext2/ext2.h"
#include "tests/ktest.h"

static void fpu_sse_init(void) {
    uint64_t cr0, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear CR0.EM (Emulation)
    cr0 |= (1ULL << 1);  // Set CR0.MP (Monitor Coprocessor)
    cr0 &= ~(1ULL << 3); // Clear CR0.TS (Task Switched)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set CR4.OSFXSR (Enable FXSAVE/FXRSTOR & SSE)
    cr4 |= (1ULL << 10); // Set CR4.OSXMMEXCPT (Enable SIMD Floating-Point Exceptions)
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));

    kprintf("[FPU/SSE] Hardware floating-point & SSE extensions enabled.\n");
}

void kernel_main(boot_info_t *boot_info) {
    uart_init();
    
    kprintf("\n======================================================\n");
    kprintf("          BangOS Bare-Metal OS Kernel (x86_64)        \n");
    kprintf("======================================================\n");
    kprintf("[Kernel] Boot info received at %p\n", boot_info);
    kprintf("[Kernel] Ramdisk payload at %p (%u bytes)\n",
            boot_info->ramdisk_paddr, (uint32_t)boot_info->ramdisk_size);

    gdt_init();
    idt_init();
    mm_init(boot_info);
    fpu_sse_init();
    syscall_init_msrs();
    process_init();

    vfs_init();
    tarfs_init(boot_info->ramdisk_paddr, boot_info->ramdisk_size);
    vfs_mount("/", "tarfs", NULL, tarfs_get_vfs_root());
    tarfs_list_files();

    ata_init();
    for (int d = 0; d < block_get_device_count(); d++) {
        block_dev_t *bdev = block_get_device_by_index(d);
        if (bdev) {
            vfs_node_t *ext2_root = NULL;
            if (ext2_mount_device(bdev, "/mnt/ext2", &ext2_root) == 0) {
                break; // Successfully mounted ext2 volume
            }
        }
    }

    ktest_run_all();

    const void *init_elf_data = NULL;
    size_t init_elf_size = 0;
    if (tarfs_lookup("/bin/init", &init_elf_data, &init_elf_size) != 0 &&
        tarfs_lookup("init", &init_elf_data, &init_elf_size) != 0) {
        init_elf_data = boot_info->elf_paddr;
        init_elf_size = boot_info->elf_size;
    }

    kprintf("[Kernel] Loading initial init ELF at %p (%u bytes)...\n",
            init_elf_data, (uint32_t)init_elf_size);

    elf_info_t elf_info;
    if (elf_load_binary(init_elf_data, init_elf_size, &elf_info) != 0) {
        kprintf("[Kernel Error] Failed to load initial user ELF payload!\n");
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }

    process_t *proc = process_create_from_elf(&elf_info, "init");
    process_jump_to_user(proc);

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}
