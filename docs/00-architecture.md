# 00 - BangOS Architecture Overview

**BangOS** is an educational 64-bit microkernel developed from scratch for the x86_64 architecture with UEFI firmware. Its primary goal is to execute native Linux C binaries (compiled with the static `musl` standard C library) directly on bare-metal hardware by implementing the Linux System Call Interface (ABI) without relying on the Linux kernel.

---

## 🏗️ System Execution Flow

The boot and execution lifecycle of BangOS follows these sequential stages:

```
+------------------------+
| 1. UEFI Firmware (OVMF)|
+------------------------+
            |
            v  Loads \EFI\BOOT\BOOTX64.EFI from FAT32 ESP
+------------------------+
| 2. UEFI Bootloader     |
+------------------------+
            |
            |-- Loads \init (Static ELF64) into physical RAM
            |-- Fetches physical memory map
            v-- Calls ExitBootServices()
+------------------------+
| 3. BangOS Kernel       |
+------------------------+
            |
            |-- Configures GDT (0x08, 0x10, 0x1B, 0x23) and TSS (0x28)
            |-- Initializes IDT (256 gates + IST stack)
            |-- Builds 4-Level Page Tables (CR3 -> 4GB Identity Map)
            |-- Enables FPU / SSE hardware support in CR0 & CR4
            |-- Configures Syscall MSRs (STAR, LSTAR, SFMASK, EFER.SCE)
            v
+------------------------+
| 4. ELF Loader Engine   |
+------------------------+
            |
            |-- Parses PT_LOAD program headers
            |-- Sets up musl C user stack (argc, argv, envp, AuxV)
            v
+------------------------+
| 5. User Process        |  <--- Syscalls (SYS_READ, SYS_WRITE, SYS_MMAP, etc.)
+------------------------+  ---> Hardware MSR syscall/sysret interface
```

---

## 📦 Core Subsystems

| Subsystem | Source Location | Description |
| :--- | :--- | :--- |
| **UEFI Bootloader** | `boot/main.c` | 64-bit UEFI application that loads the ELF binary into RAM and exits Boot Services. |
| **GDT & TSS** | `kernel/arch/x86_64/gdt.c` | Code and data segment descriptors for Kernel space (Ring 0) and User space (Ring 3). |
| **IDT & Exceptions** | `kernel/arch/x86_64/idt.c` | Central dispatcher for all 256 x86_64 CPU interrupts and exceptions. |
| **Paging & MM** | `kernel/mm/memory.c` | 4KB physical frame allocator and 4-level Page Table manager (PML4, PDPT, PD, PT). |
| **FPU/SSE Initializer**| `kernel/main.c` | Configures CR0/CR4 control registers for hardware SIMD floating-point execution. |
| **ELF64 Loader** | `kernel/loader/elf.c` | Parser for 64-bit ELF executable binaries. |
| **Process Manager** | `kernel/process/process.c` | Prepares user memory space and initial C ABI stack layout. |
| **Syscall Engine** | `kernel/syscall/syscall.c` | Linux x86_64 ABI system call dispatcher. |
| **16550 UART Driver** | `kernel/drivers/uart.c` | Serial console driver over COM1 (`0x3F8`) at 38400 baud. |
