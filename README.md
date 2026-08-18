# BangOS 💥

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: x86_64](https://img.shields.io/badge/Architecture-x86__64-orange.svg)]()
[![Firmware: UEFI](https://img.shields.io/badge/Firmware-UEFI-green.svg)]()
[![C Runtime: musl](https://img.shields.io/badge/C--Runtime-musl-informational.svg)]()
[![Documentation](https://img.shields.io/badge/Documentation-GitHub_Pages-brightgreen.svg)](https://vikman90.github.io/bangos/)

**BangOS** is an educational, lightweight 64-bit bare-metal operating system microkernel designed to run standard Linux binaries (compiled statically with the `musl` standard C library) directly on x86_64 UEFI hardware without requiring the Linux kernel.

The repository serves as a comprehensive, clean foundation for learning low-level OS development, 64-bit UEFI bootloading, x86_64 CPU initialization (GDT, TSS, IDT), 4-level paging with Demand Paging, FPU/SSE SIMD extensions, preemptive multitasking, and the Linux System Call ABI.

---

## 🌟 Core Subsystems & Key Features

* **64-bit UEFI Bootloader (`BOOTX64.EFI`)**: Loads directly via OVMF UEFI firmware, sets up graphics/serial console, reads `initrd.tar` from FAT32 partitions, queries memory maps, and executes `ExitBootServices()`.
* **In-Memory TarFS Ramdisk**: Parses standard USTAR archives containing standalone 64-bit ELF binaries directly in physical RAM.
* **x86_64 Core Microkernel**:
  * **GDT & TSS**: Segment descriptors configured for Kernel space (`0x08`, `0x10`) and User space (`0x1B`, `0x23`) with dynamic `RSP0` stack switching.
  * **IDT & Exception Handling**: 256 interrupt gates with dedicated Interrupt Stack Table (`IST1`) for fault safety (`#DF`, `#GP`).
  * **4-Level Paging & VMM**: PML4, PDPT, PD, PT hierarchy with 4 GB identity mapping, physical frame bitmap allocator, and per-process Virtual Memory Areas (VMAs).
  * **Demand Paging**: Zero-fill on-demand page allocation handling Page Fault exceptions (`#PF`, Vector 14) for dynamic `mmap()`.
  * **FPU & SSE SIMD Extensions**: `%cr0` / `%cr4` configured for 64-bit hardware floating-point execution (`sqrt`, double precision math, `fxsave64`/`fxrstor64` context switching).
* **Preemptive Multitasking & Multithreading**:
  * **8254 PIT Timer Driver**: Generates 100 Hz timer interrupts (10 ms time quantum) for Round-Robin process scheduling.
  * **Process Lifecycle**: `SYS_FORK`, `SYS_CLONE` (with `CLONE_VM`), `SYS_EXECVE`, `SYS_WAIT4`, and `SYS_EXIT`.
  * **Fast Userspace Locking**: `SYS_FUTEX` supporting `FUTEX_WAIT` and `FUTEX_WAKE`.
* **Block Storage & ext2 Filesystem Engine**:
  * **ATA / IDE PIO Driver**: Hardware controller probing, 16-bit PIO sector read/write with LBA28 and LBA48 support.
  * **Virtual File System (VFS)**: Hierarchical VFS supporting multiple mountpoints (`/` for TarFS ramdisk, `/mnt/ext2` for persistent storage), path resolution, and file descriptors.
  * **ext2 Filesystem Engine**: Superblock parsing (`0xEF53`), block group descriptor tables, inode tables, direct/indirect block resolution, directory lookup, and block write allocation.
* **Linux x86_64 Syscall ABI**: Hardware `syscall` / `sysret` MSR interface (`STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`) implementing over 25 POSIX syscalls (`open`, `close`, `read`, `write`, `lseek`, `stat`, `fstat`, `getdents64`, `mmap`, `mprotect`, `munmap`, `brk`, `poll`, `ioctl`, `nanosleep`, `sysinfo`, `uname`, `arch_prctl`, etc.).
* **Two-Tier Test Verification**:
  * **Ring 0 `ktest`**: In-kernel unit tests verifying memory allocators, page tables, TarFS, scheduler, ATA PIO, and ext2.
  * **Ring 3 POSIX Specs**: Standalone userland test suites validating syscall safety, demand paging, process lifecycles, and ext2 file operations.
* **Standalone Multi-ELF Userland**:
  * **`/bin/init` (PID 1)**: Interactive process supervisor and launcher.
  * **`/bin/calc`**: Standalone geometric calculator.
  * **`/bin/sysinfo`**: Standalone hardware and operating system report.
  * **`/bin/bench`**: Multi-phase CPU, FPU/SSE, dynamic memory, and demand paging benchmark suite.
  * **`/bin/tasks`**: Preemptive multitasking and concurrent computation workers demo.
  * **`/bin/threads`**: Multithreading, mutexes, counting semaphores, and futex synchronization suite.
  * **`/bin/disktool`**: Interactive storage explorer, superblock inspector, and persistence tester.

---

## 📚 Technical Documentation Syllabus

The interactive documentation website is published online at **[https://vikman90.github.io/bangos/](https://vikman90.github.io/bangos/)**.

Comprehensive documentation explaining the theoretical concepts, hardware specifications, and code implementations is available in the [`docs/`](docs/) directory:

| Chapter | Title | Key Topics Covered |
| :--- | :--- | :--- |
| [**00 - Architecture Overview**](docs/00-architecture.md) | System Architecture & Design Philosophy | Microkernel design, boot flow, Ring 0 vs Ring 3, memory map, subsystem interactions. |
| [**01 - UEFI Bootloading**](docs/01-uefi-bootloading.md) | 64-bit UEFI Bootloading & Firmware Handoff | GNU-EFI, FAT32 ESP traversal, `initrd.tar` loading, memory map acquisition, `ExitBootServices`. |
| [**02 - GDT, TSS & IDT**](docs/02-gdt-idt-tss.md) | Segmentation, GDT, TSS & Exception Handling | Long Mode segmentation, segment selectors, TSS `rsp0`/`ist1`, 256-gate IDT, 8259 PIC remapping. |
| [**03 - Paging & Memory Management**](docs/03-paging-and-memory.md) | 4-Level Paging, Allocators & Demand Paging | PML4/PDPT/PD/PT translation, 4GB identity mapping, frame bitmap allocator, VMAs, Demand Paging `#PF`. |
| [**04 - System Call ABI**](docs/04-syscall-abi.md) | System Call Subsystem (Linux x86_64 ABI) | Hardware MSRs (`STAR`, `LSTAR`, `SFMASK`), `syscall_entry.s` trampoline, exhaustive syscall reference table. |
| [**05 - Process Management & Scheduling**](docs/05-process-multitasking-sched.md) | Process Management, Preemptive Sched & Futex | PCB (`process_t`), `fork()`, `clone()`, musl user stack, 100 Hz PIT timer, Round-Robin scheduler, `futex`. |
| [**06 - In-Memory TarFS & ELF64 Loader**](docs/06-elf-loader-tarfs.md) | In-Memory TarFS Ramdisk & ELF64 Loader | USTAR format, octal parsing, `Elf64_Ehdr`, `Elf64_Phdr`, `PT_LOAD` mapping, BSS zeroing. |
| [**07 - FPU & SSE Extensions**](docs/07-fpu-sse.md) | FPU & SIMD/SSE Floating-Point Support | CR0/CR4 control registers, `fninit`, `fxsave64`/`fxrstor64` context switching, 16-byte stack alignment. |
| [**08 - Hardware Device Drivers**](docs/08-hardware-drivers.md) | Hardware Device Drivers & Low-Level I/O | 16550 UART serial COM1 driver, 8254 PIT timer, PS/2 keyboard controller, QEMU debug/poweroff ports. |
| [**09 - Userland Environment**](docs/09-userland-environment.md) | Userland Runtime, Applications & Concurrency | Static `musl-gcc`, `/bin/init` supervisor, standalone ELFs, `tui.h` ANSI library, `synch.h` concurrency. |
| [**10 - Specification Testing & QEMU**](docs/10-testing-and-qemu.md) | Specification-Driven Testing & QEMU Harness | Ring 0 `ktest` unit tests, Ring 3 POSIX specification suites, headless QEMU TCP serial test runner. |
| [**11 - Extending BangOS**](docs/11-extending-bangos.md) | Extending BangOS Developer Guide | Step-by-step developer tutorial for adding new syscalls, drivers, userland tools, and test suites. |
| [**12 - ATA / IDE Storage Driver**](docs/12-ata-storage-driver.md) | ATA / IDE Storage Driver Architecture | Port I/O register maps, PIO mode, LBA28/48, sector read/write protocols, block device abstraction. |
| [**13 - VFS & ext2 Filesystem**](docs/13-vfs-and-ext2.md) | Virtual File System & ext2 Filesystem Engine | Superblock, block groups, inodes, direct/indirect mapping, directory records, and POSIX syscall mappings. |

---

## 🖥️ Multi-ELF Userland Environment

```text
======================================================================
        BangOS (x86_64) - Bare Metal Kernel v0.2.0        
     PID: 1 (init) | RAM: 128 MB Total (126 MB Free) | Uptime: 0 s
======================================================================

Available Standalone Applications (Multi-ELF Ramdisk):

  [1] Geometric Calculator           (execve /bin/calc)
  [2] System Information & Uname     (execve /bin/sysinfo)
  [3] CPU FPU/SSE & Timer Benchmark  (execve /bin/bench)
  [4] Preemptive Multitasking Tasks  (execve /bin/tasks)
  [5] Multithreading & Mutex Sync    (execve /bin/threads)
  [6] Disk Explorer & Storage Mgr    (execve /bin/disktool)
  [7] Run Specification Test Suites  (execve /bin/test_*)
  [8] Shutdown / Halt System         (exit)

Select an option [1-8]: 
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
    tar \
    python3
```

---

## 🚀 Building and Running

### 1. Build the OS & Bootloader
Compile userland binaries, assemble `initrd.tar`, compile the EFI kernel binary, and generate the FAT32 boot filesystem at `build/esp/`:

```bash
make esp
```

### 2. Run under QEMU
Launch BangOS in QEMU using OVMF UEFI firmware:

```bash
make run-qemu
```

### 3. Automated Test Suite
Execute the headless integration test harness with TCP socket serial communication:

```bash
make test
```

### 4. Docker / macOS Cross-Compilation
For building and testing in isolated containers or on macOS:

```bash
# Build OS artifacts inside Docker
make docker-build

# Run automated integration tests inside Docker
make docker-test

# Open interactive bash in the build container
make docker-shell
```

---

## 📁 Repository Structure

```text
BangOS/
├── Makefile                     # Root build system
├── LICENSE                      # MIT License
├── README.md                    # Project overview and index
├── AGENTS.md                    # AI agent guidelines & engineering standards
├── boot/                        # 64-bit UEFI Bootloader
│   └── main.c                   # Bootloader entrypoint & firmware handoff
├── docs/                        # Subsystem technical documentation
│   ├── 00-architecture.md
│   ├── 01-uefi-bootloading.md
│   ├── 02-gdt-idt-tss.md
│   ├── 03-paging-and-memory.md
│   ├── 04-syscall-abi.md
│   ├── 05-process-multitasking-sched.md
│   ├── 06-elf-loader-tarfs.md
│   ├── 07-fpu-sse.md
│   ├── 08-hardware-drivers.md
│   ├── 09-userland-environment.md
│   ├── 10-testing-and-qemu.md
│   └── 11-extending-bangos.md
├── include/                     # Kernel header definitions
│   ├── kernel.h                 # Boot structures and MSR macros
│   └── uefi.h
├── kernel/                      # 64-bit OS Microkernel
│   ├── main.c                   # Kernel main orchestrator & FPU init
│   ├── arch/x86_64/             # GDT, TSS, IDT, and Syscall assembly
│   ├── drivers/                 # 16550 UART, 8254 PIT, 8259 PIC, PS/2, QEMU
│   ├── fs/                      # In-Memory TarFS ramdisk driver
│   ├── lib/                     # Freestanding kstring library
│   ├── loader/                  # ELF64 binary loader
│   ├── mm/                      # PMM frame allocator, Paging & VMM
│   ├── process/                 # Process table, fork, clone, scheduler, futex
│   ├── syscall/                 # Linux x86_64 syscall dispatcher
│   └── tests/                   # Ring 0 in-kernel unit test suite (ktest)
├── scripts/
│   ├── run_qemu_test.sh         # Test execution shell wrapper
│   └── test_runner.py           # Automated TCP socket QEMU test harness
└── userland/                    # Standalone C Userland & initrd packaging
    ├── Makefile                 # Userland compilation & USTAR tar rules
    ├── include/                 # Userland headers (tui.h, synch.h)
    └── src/                     # Standalone applications & POSIX test suites
```

---

## 🗺️ Roadmap & Subsystem Status

- [x] 64-bit UEFI Bootloader with memory map parsing & `ExitBootServices`
- [x] x86_64 GDT, TSS (`rsp0`, `ist1`), IDT (256 gates), FPU/SSE SIMD support
- [x] 4-Level Paging (CR3) with 4GB Identity Mapping & 4KB Bitmap Frame Allocator
- [x] Virtual Memory Area (VMA) Manager & On-Demand Paging on `#PF` (Vector 14)
- [x] In-Memory TarFS Ramdisk Driver (`initrd.tar`) & ELF64 Program Header Loader
- [x] Multi-Process Lifecycle (`SYS_FORK`, `SYS_EXECVE`, `SYS_WAIT4`, `SYS_EXIT`)
- [x] Preemptive Multitasking & Context Switching (8254 PIT 100 Hz / 10 ms time slice)
- [x] Multithreading (`CLONE_VM`), Thread Local Storage (`FS_BASE`), and `SYS_FUTEX`
- [x] Standalone Multi-ELF Userland Suite & POSIX Specification Test Harness
- [x] Two-Tier Automated Verification Architecture (Ring 0 `ktest` + Ring 3 POSIX)
- [x] ATA / IDE Storage Drive Driver, VFS Multi-Mount & ext2 Filesystem Engine
- [ ] VirtIO Network Driver & Lightweight TCP/IP Stack

---

## 📄 License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
