# BangOS 💥

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: x86_64](https://img.shields.io/badge/Architecture-x86__64-orange.svg)]()
[![Firmware: UEFI](https://img.shields.io/badge/Firmware-UEFI-green.svg)]()
[![C Runtime: musl](https://img.shields.io/badge/C--Runtime-musl-informational.svg)]()

**BangOS** is a lightweight, educational 64-bit bare-metal operating system kernel designed to run standard Linux binaries (compiled with `musl` C runtime) directly on x86_64 UEFI hardware without requiring the Linux kernel.

The project is designed to be highly modular and extensible, serving as a clean foundation for learning low-level OS development, UEFI bootloading, x86_64 CPU initialization, paging, FPU/SSE extensions, and the Linux System Call Interface (ABI).

---

## 🌟 Key Features

* **64-bit UEFI Bootloader**: Loads directly via OVMF UEFI firmware, sets up graphics/serial console, reads the `initrd.tar` ramdisk from FAT32 boot partitions, and transitions to kernel mode.
* **In-Memory TarFS Ramdisk**: Parses standard USTAR archives containing standalone ELF binaries directly in physical RAM.
* **x86_64 Core Subsystems**:
  * **GDT & TSS**: Segment descriptors configured for kernel space (`0x08`, `0x10`) and user space (`0x1B`, `0x23`).
  * **IDT & Exception Handling**: Interrupt table with 256 gates and dedicated Interrupt Stack Table (IST) for exception safety.
  * **Paging & Memory Management**: 4-level Page Tables (PML4, PDPT, PD, PT) with 4GB identity mapping and dynamic user page mapping.
  * **FPU & SSE Support**: `%cr0` / `%cr4` configured for SIMD 64-bit floating-point math execution (`sqrt`, double precision).
* **Linux x86_64 Syscall Interface**: Supports the native hardware `syscall` / `sysret` instruction interface (MSRs `STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`).
* **musl C System Calls Implemented**:
  * `SYS_READ` (`0`): Console/Serial stdin input.
  * `SYS_WRITE` (`1`) / `SYS_WRITEV` (`20`): Serial stdout output and text formatting.
  * `SYS_POLL` (`7`): Non-blocking I/O polling.
  * `SYS_MMAP` (`9`) / `SYS_BRK` (`12`): Memory allocation and user heap growth.
  * `SYS_NANOSLEEP` (`35`): High-precision sleep with timestamp counter (TSC).
  * `SYS_GETPID` (`39`): Process ID query.
  * `SYS_FORK` (`57`) / `SYS_VFORK` (`58`) / `SYS_CLONE` (`56`): Process context creation.
  * `SYS_EXECVE` (`59`): Replaces process image with standalone ELF from TarFS ramdisk.
  * `SYS_WAIT4` (`61`): Waits for child process termination.
  * `SYS_UNAME` (`63`): System name, version, and architecture query (`struct utsname`).
  * `SYS_SYSINFO` (`99`): System statistics, uptime, and memory usage (`struct sysinfo`).
  * `SYS_ARCH_PRCTL` (`158`): Thread Local Storage (`FS_BASE` / `GS_BASE`).
  * `SYS_IOCTL` (`16`): Terminal attributes (`TIOCGWINSZ`).
  * `SYS_CLOCK_GETTIME` (`228`): Monotonic and realtime timestamps with nanosecond precision.
  * `SYS_EXIT` (`60`) / `SYS_EXIT_GROUP` (`231`): Process termination, child reaping, or system halt.
* **Standalone Multi-ELF Userland**:
  * **`/bin/init` (PID 1)**: Interactive process launcher using `fork()`, `execve()`, and `waitpid()`.
  * **`/bin/calc`**: Standalone geometric calculator executable.
  * **`/bin/sysinfo`**: Standalone system hardware and OS report executable.
  * **`/bin/bench`**: Standalone CPU, FPU/SSE, memory allocation, and timer benchmark executable.
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

## 🖥️ Multi-ELF Userland Environment

```text
======================================================================
        BangOS (x86_64) - Bare Metal Kernel v0.2.0        
     PID: 1 (init) | RAM: 128 MB Total (127 MB Free) | Uptime: 0 s
======================================================================

Available Standalone Applications (Multi-ELF Ramdisk):

  [1] Geometric Calculator           (execve /bin/calc)
  [2] System Information & Uname     (execve /bin/sysinfo)
  [3] CPU FPU/SSE & Timer Benchmark  (execve /bin/bench)
  [4] Shutdown / Halt System         (exit)

Select an option [1-4]: 
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

To compile the user applications, build the `initrd.tar` ramdisk, bootloader, kernel, and create the FAT ESP image:

```bash
make esp
```

### 2. Run under QEMU

Launch BangOS in QEMU using OVMF UEFI firmware:

```bash
make run-qemu
```

### 3. Automated Test Suite

Run the integration test suite that spawns QEMU, passes inputs over a TCP serial socket, and verifies multi-process execution:

```bash
make test
```

### 4. Docker / macOS Workflow (Apple Silicon M-Series & Cross-Compile)

For building on macOS or environments without bare-metal Linux toolchains:

```bash
# Build OS artifacts inside Docker container
make docker-build

# Run automated integration tests inside Docker
make docker-test

# Open interactive bash inside the build container
make docker-shell
```

---

## 📁 Repository Structure

```
BangOS/
├── Makefile                     # Root build system
├── LICENSE                      # MIT License
├── README.md                    # Project documentation
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
│   ├── fs/                      # In-Memory TarFS ramdisk driver
│   ├── loader/                  # ELF64 binary loader
│   ├── mm/                      # Physical frame allocator & 64-bit Paging
│   ├── process/                 # Multi-process manager (fork, execve, wait4)
│   └── syscall/                 # Linux x86_64 syscall handlers
├── scripts/
│   ├── run_qemu_test.sh         # Shell runner wrapper
│   └── test_runner.py           # Automated test runner with socket I/O
└── userland/                    # Standalone C Userland applications & initrd
    ├── Makefile
    ├── include/
    │   └── tui.h
    └── src/
        ├── init.c               # Standalone PID 1 process launcher
        ├── calc.c               # Standalone geometric calculator binary
        ├── sysinfo.c            # Standalone system info & hardware report binary
        ├── bench.c              # Standalone CPU & memory benchmark binary
        └── tui.c                # Shared terminal UI library
```

---

## 🗺️ Roadmap & Future Enhancements

- [x] 64-bit UEFI Bootloader
- [x] x86_64 GDT, TSS, IDT, Paging (CR3), FPU/SSE support
- [x] ELF64 Loader & Musl Linux System Call Engine
- [x] In-Memory TarFS Ramdisk Driver (`initrd.tar`)
- [x] Multi-ELF Process Execution (`SYS_FORK`, `SYS_EXECVE`, `SYS_WAIT4`)
- [ ] Preemptive Multitasking & Context Switching
- [ ] Virtual Memory Manager with Demand Paging & `mmap` backing
- [ ] ATA / AHCI Storage Drive Driver & FAT32/ext2 Filesystem
- [ ] VirtIO Network Driver & Lightweight TCP/IP Stack

---

## 📄 License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
