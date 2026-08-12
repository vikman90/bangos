# BangOS 💥

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: x86_64](https://img.shields.io/badge/Architecture-x86__64-orange.svg)]()
[![Firmware: UEFI](https://img.shields.io/badge/Firmware-UEFI-green.svg)]()
[![C Runtime: musl](https://img.shields.io/badge/C--Runtime-musl-informational.svg)]()

**BangOS** is a lightweight, educational 64-bit bare-metal operating system kernel designed to run standard Linux binaries (compiled with `musl` C runtime) directly on x86_64 UEFI hardware without requiring the Linux kernel.

The project is designed to be highly modular and extensible, serving as a clean foundation for learning low-level OS development, UEFI bootloading, x86_64 CPU initialization, paging, FPU/SSE extensions, and the Linux System Call Interface (ABI).

---

## 🌟 Key Features

* **64-bit UEFI Bootloader**: Loads directly via OVMF UEFI firmware, sets up graphics/serial console, reads ELF executables from FAT32 boot partitions, and transitions to kernel mode.
* **x86_64 Core Subsystems**:
  * **GDT & TSS**: Segment descriptors configured for kernel space (`0x08`, `0x10`) and user space (`0x1B`, `0x23`).
  * **IDT & Exception Handling**: Interrupt table with 256 gates and dedicated Interrupt Stack Table (IST) for exception safety.
  * **Paging & Memory Management**: 4-level Page Tables (PML4, PDPT, PD, PT) with 4GB identity mapping and dynamic user page mapping.
  * **FPU & SSE Support**: `%cr0` / `%cr4` configured for SIMD 64-bit floating-point math execution (`sqrt`, double precision).
* **Linux x86_64 Syscall Interface**: Supports the native hardware `syscall` / `sysret` instruction interface (MSRs `STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`).
* **musl C System Calls Implemented**:
  * `SYS_READ` (`0`): Console/Serial stdin input.
  * `SYS_WRITE` (`1`) / `SYS_WRITEV` (`20`): Serial stdout output and text formatting.
  * `SYS_MMAP` (`9`) / `SYS_BRK` (`12`): Memory allocation and user heap growth.
  * `SYS_ARCH_PRCTL` (`158`): Thread Local Storage (`FS_BASE` / `GS_BASE`).
  * `SYS_IOCTL` (`16`): Terminal attributes (`TIOCGWINSZ`).
  * `SYS_EXIT` (`60`) / `SYS_EXIT_GROUP` (`231`): Clean process termination and system halt.
* **16550 UART Driver**: Full serial console support over COM1 (`0x3F8`).

---

## 📚 Documentation Guides

Detailed technical documentation for every subsystem is available in the [`docs/`](docs/) directory:

1. [**00 - Architecture Overview**](docs/00-architecture.md): Overall system design, component breakdown, and boot control flow.
2. [**01 - UEFI Bootloading**](docs/01-uefi-bootloading.md): 64-bit UEFI bootloader (`BOOTX64.EFI`), memory map retrieval, and ExitBootServices transition.
3. [**02 - GDT, TSS & IDT**](docs/02-gdt-idt-tss.md): x86_64 segmentation, segment descriptors, TSS, IST exception stacks, and CPU fault handlers.
4. [**03 - Paging & Memory Management**](docs/03-paging-and-memory.md): 4-level page table structure (PML4, PDPT, PD, PT), 4GB identity mapping, frame allocation, and TLB invalidation.
5. [**04 - System Call ABI**](docs/04-syscall-abi.md): MSR configuration (`STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`), System V AMD64 ABI, and musl syscall handling table.
6. [**05 - FPU & SSE Extensions**](docs/05-fpu-sse.md): Hardware CR0/CR4 configuration enabling 64-bit SIMD floating-point math (`sqrt`, double precision).
7. [**06 - Testing & QEMU Automation**](docs/06-testing-and-qemu.md): QEMU TCP serial socket configuration, Python test runner, and CI test harness.
8. [**07 - Extending BangOS**](docs/07-extending-bangos.md): Step-by-step developer guide to adding new system calls, hardware drivers, and user applications.

---

## 📐 Initial Userland Application: `calc`

To prove bare-metal execution of unmodified C binaries, BangOS loads and runs `app/calc`, an initial static C userland program built with `musl-gcc` that calculates mathematical functions using standard `stdio` and `math.h` (`sqrt()`):

```text
======================================================
          BangOS Bare-Metal OS Kernel (x86_64)        
======================================================
[Kernel] Boot info received at 1fe96680
[GDT] 64-bit GDT and TSS loaded successfully.
[IDT] 64-bit IDT initialized with 256 gates.
[MM] Dedicated 64-bit Paging CR3 loaded (2000000), 4GB identity mapped.
[FPU/SSE] Hardware floating-point & SSE extensions enabled.
[ELF] All segments loaded successfully.
[Process] Launching ELF process execution (RIP=401108, RSP=7fffefffff00) ...

Enter first side: 3
Enter second side: 4
Hypotenuse: 5.00

[Kernel] Process exited with status code: 0
[Kernel] System halting cleanly.
```

---

## 🛠️ Requirements & Dependencies

On Debian/Ubuntu-based Linux systems:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gnu-efi \
    nasm \
    qemu-system-x86 \
    ovmf \
    musl \
    musl-tools \
    python3
```

---

## 🚀 Building and Running

### 1. Build the OS & Bootloader

To compile the user application, bootloader, kernel, and create the FAT ESP image:

```bash
make esp
```

### 2. Run under QEMU

Launch BangOS in QEMU using OVMF UEFI firmware:

```bash
make run-qemu
```

### 3. Automated Test Suite

Run the integration test suite that spawns QEMU, passes inputs over a TCP serial socket, and verifies execution:

```bash
make test
```

---

## 📁 Repository Structure

```
BangOS/
├── Makefile                     # Root build system
├── LICENSE                      # MIT License
├── README.md                    # Project documentation
├── app/                         # Initial userland application (calc)
│   ├── Makefile
│   └── calc.c
├── boot/                        # 64-bit UEFI Bootloader
│   └── main.c
├── docs/                        # Subsystem documentation guides
│   ├── 00-architecture.md
│   ├── 01-uefi-bootloading.md
│   ├── 02-gdt-idt-tss.md
│   ├── 03-paging-and-memory.md
│   ├── 04-syscall-abi.md
│   ├── 05-fpu-sse.md
│   ├── 06-testing-and-qemu.md
│   └── 07-extending-bangos.md
├── include/                     # Kernel headers
│   ├── kernel.h
│   └── uefi.h
├── kernel/                      # 64-bit OS Kernel
│   ├── main.c
│   ├── arch/x86_64/             # GDT, IDT, Syscall assembly
│   ├── drivers/                 # 16550 UART serial & Keyboard drivers
│   ├── loader/                  # ELF64 binary loader
│   ├── mm/                      # Physical frame allocator & 64-bit Paging
│   ├── process/                 # User process manager
│   └── syscall/                 # Linux x86_64 syscall handlers
└── scripts/
    ├── run_qemu_test.sh         # Shell runner wrapper
    └── test_runner.py           # Automated test runner with socket I/O
```

---

## 🗺️ Roadmap & Future Enhancements

- [x] 64-bit UEFI Bootloader
- [x] x86_64 GDT, TSS, IDT, Paging (CR3), FPU/SSE support
- [x] ELF64 Loader & Musl Linux System Call Engine
- [ ] Preemptive Multitasking & Context Switching
- [ ] Virtual Memory Manager with Demand Paging & `mmap` backing
- [ ] ATA / AHCI Storage Drive Driver & FAT32/ext2 Filesystem
- [ ] VirtIO Network Driver & Lightweight TCP/IP Stack

---

## 📄 License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
