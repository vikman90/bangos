# 00 - BangOS Architecture & System Design

**BangOS** is an educational, 64-bit operating system kernel developed from scratch for the x86_64 architecture with modern UEFI firmware. Its primary goal is to run native, statically compiled Linux C binaries (built with the `musl` standard library) directly on bare-metal hardware by implementing the Linux System Call Interface (ABI) without requiring the Linux kernel.

---

## 🎯 Design Philosophy & Core Principles

1. **Bare-Metal Simplicity & Educational Clarity**:
   BangOS avoids unnecessary abstractions. Every subsystem—from CPU segment initialization to memory management, task scheduling, and system calls—is implemented with direct register manipulations, clear data structures, and readable C/assembly code.

2. **Ring 0 / Ring 3 Hardware Privilege Separation**:
   The CPU operates in Long Mode with two distinct privilege levels:
   - **Ring 0 (Kernel Space / CPL 0)**: Executes the microkernel, physical memory manager, paging infrastructure, hardware drivers (UART, PIT, PS/2), and exception dispatchers.
   - **Ring 3 (User Space / CPL 3)**: Executes standalone ELF binaries compiled against the static `musl` C library (`/bin/init`, `/bin/calc`, `/bin/sysinfo`, `/bin/bench`, `/bin/tasks`, `/bin/threads`, and test suites).

3. **Binary Compatibility with Linux ABI**:
   Userland programs do not link against custom kernel stubs. Instead, standard `musl-gcc` binaries invoke the native x86_64 hardware instruction `syscall`. BangOS catches these calls in Ring 0, translates arguments according to the System V AMD64 ABI, and fulfills POSIX semantics.

4. **Self-Contained Ramdisk Execution**:
   BangOS packages userland executables into a standard USTAR archive (`initrd.tar`) loaded into memory at boot time by UEFI. An in-memory TarFS driver parses and loads executables on demand without requiring disk block drivers.

---

## 🏗️ End-to-End System Execution Lifecycle

```text
+-----------------------------------------------------------------------------------+
| 1. UEFI Firmware (OVMF / Physical PC)                                             |
|    - Initializes motherboard chipset, CPUs, and FAT32 EFI System Partition (ESP)  |
|    - Discovers and executes \EFI\BOOT\BOOTX64.EFI                                 |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 2. BangOS UEFI Bootloader (boot/main.c)                                           |
|    - Uses UEFI SimpleFileSystemProtocol to locate and read \initrd.tar into RAM   |
|    - Retrieves the physical UEFI Memory Map via GetMemoryMap()                    |
|    - Calls ExitBootServices() to terminate firmware runtime services              |
|    - Transfers control and boot_info_t pointer to kernel_main()                   |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 3. BangOS Microkernel Initialization (kernel/main.c)                              |
|    - [UART] Initializes 16550 Serial Console on COM1 (0x3F8) at 38400 baud        |
|    - [GDT/TSS] Configures 64-bit descriptors and TSS with RSP0 and IST1 stacks    |
|    - [IDT] Sets up 256 interrupt gates, remaps 8259 PIC, programs 8254 PIT (100Hz)|
|    - [MM/PMM] Builds 4-Level Page Tables (PML4) with 4GB identity mapping         |
|    - [FPU/SSE] Enables CR0.MP, clears CR0.EM/TS, sets CR4.OSFXSR & OSXMMEXCPT     |
|    - [Syscall] Configures MSRs (STAR, LSTAR, SFMASK, EFER.SCE) for fast syscalls  |
|    - [Process] Initializes Process Table, Round-Robin structures, and Futex queue |
|    - [TarFS] Mounts USTAR ramdisk in memory and validates all archive contents    |
|    - [KTest] Executes Ring 0 in-kernel unit test suite (PMM, VMM, Strings, Sched) |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 4. Initial Process Launch (kernel/loader/elf.c & kernel/process/process.c)        |
|    - Looks up /bin/init in TarFS ramdisk                                          |
|    - Parses ELF64 header and allocates memory for PT_LOAD segments                |
|    - Constructs musl user stack (argc=1, argv=["init"], envp=[NULL], AuxV=[NULL]) |
|    - Sets up user context frame (RIP=entry, RSP=user_rsp, CS=0x23, SS=0x1B)       |
|    - Jumps to Ring 3 userland via iretq / switch_to_context_frame()               |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 5. Multi-ELF Userland Ecosystem (userland/src/init.c & standalone binaries)       |
|    - PID 1 (/bin/init) displays interactive system menu                           |
|    - Spawns child processes using fork(), execve(), and waitpid()                 |
|    - Preemptive 100 Hz PIT timer switches contexts every 10 ms quantum            |
|    - Processes allocate memory dynamically using mmap(), mprotect(), munmap(), brk|
|    - Multithreading runs via sys_clone (CLONE_VM) & sys_futex (FUTEX_WAIT/WAKE)   |
+-----------------------------------------------------------------------------------+
```

---

## 📦 Subsystem Breakdown & Repository Layout

| Subsystem | Primary Source Location | Key Responsibility |
| :--- | :--- | :--- |
| **UEFI Bootloader** | [`boot/main.c`](file:///root/test/little-bang/boot/main.c) | 64-bit EFI application loading `initrd.tar`, collecting memory maps, and executing the kernel. |
| **Kernel Entrypoint** | [`kernel/main.c`](file:///root/test/little-bang/kernel/main.c) | Master boot sequencer orchestrating all kernel subsystem initializations. |
| **GDT & TSS** | [`kernel/arch/x86_64/gdt.c`](file:///root/test/little-bang/kernel/arch/x86_64/gdt.c) | Global Descriptor Table (64-bit selectors) and Task State Segment (`rsp0`, `ist1`). |
| **IDT & Exceptions** | [`kernel/arch/x86_64/idt.c`](file:///root/test/little-bang/kernel/arch/x86_64/idt.c) | 256-gate Interrupt Descriptor Table, exception frame dumps, and interrupt routing. |
| **Low-Level ISRs** | [`kernel/arch/x86_64/isr.s`](file:///root/test/little-bang/kernel/arch/x86_64/isr.s) | Assembly interrupt service routine stubs and context frame construction. |
| **Syscall Entrypoint** | [`kernel/arch/x86_64/syscall_entry.s`](file:///root/test/little-bang/kernel/arch/x86_64/syscall_entry.s) | Hardware MSR `syscall`/`sysret` assembly trampoline and stack switcher. |
| **Memory Manager (PMM)**| [`kernel/mm/memory.c`](file:///root/test/little-bang/kernel/mm/memory.c) | Bitmap physical frame allocator and 4-level x86_64 paging tables (PML4, PDPT, PD, PT). |
| **Virtual Memory (VMM)**| [`kernel/mm/vmm.c`](file:///root/test/little-bang/kernel/mm/vmm.c) | Per-process Virtual Memory Areas (VMAs), Demand Paging on `#PF` (vector 14), and `mmap`. |
| **Process & Scheduler** | [`kernel/process/process.c`](file:///root/test/little-bang/kernel/process/process.c) | Process table, `fork()`, `clone()`, `execve()`, `wait4()`, Round-Robin scheduler, and `futex`. |
| **TarFS Ramdisk** | [`kernel/fs/tarfs.c`](file:///root/test/little-bang/kernel/fs/tarfs.c) | In-memory USTAR archive parser and binary file lookup engine. |
| **ELF64 Loader** | [`kernel/loader/elf.c`](file:///root/test/little-bang/kernel/loader/elf.c) | Executable and Linkable Format parser, `PT_LOAD` mapper, and BSS clearing. |
| **Syscall Dispatcher** | [`kernel/syscall/syscall.c`](file:///root/test/little-bang/kernel/syscall/syscall.c) | Linux x86_64 ABI system call dispatcher handling over 35 POSIX syscalls. |
| **Storage & VFS** | [`kernel/fs/`](file:///root/test/little-bang/kernel/fs/) | Virtual File System mountpoints, ATA PIO block storage driver, and ext2 filesystem engine. |
| **Hardware Drivers** | [`kernel/drivers/`](file:///root/test/little-bang/kernel/drivers/) | 16550 UART serial, 8254 PIT timer, 8259 PIC, PS/2 keyboard, ATA PIO, PCI scanner, and VirtIO-Net. |
| **Network Stack** | [`kernel/net/`](file:///root/test/little-bang/kernel/net/) | In-kernel TCP/IP protocol stack (Ethernet II, ARP, IPv4, ICMP, UDP, DNS, TCP, and POSIX sockets). |
| **Kernel Library** | [`kernel/lib/kstring.c`](file:///root/test/little-bang/kernel/lib/kstring.c) | Freestanding string manipulation and memory copying routines for Ring 0. |
| **Kernel Tests (`ktest`)**| [`kernel/tests/`](file:///root/test/little-bang/kernel/tests/) | Built-in Ring 0 unit test suites (PMM, VMM, Strings, Sched, ATA, ext2, Net). |
| **Userland Binaries** | [`userland/src/`](file:///root/test/little-bang/userland/src/) | Applications (`init`, `calc`, `sysinfo`, `bench`, `tasks`, `threads`, `disktool`, `netfetch`, and POSIX test suites). |

---

## 🗺️ Physical and Virtual Address Space Map

BangOS maps physical memory into a predictable virtual layout:

```text
+-----------------------+ 0xFFFFFFFFFFFFFFFF (Top of 64-bit Virtual Space)
|                       |
|   Unused / Canonical  |
|                       |
+-----------------------+ 0x00007FFFFFFFF000 (USER_STACK_TOP)
|   User Stack (64 KB)  | (Grows downwards, per-process mapping)
+-----------------------+ 0x00007FFFFFFEF000
|                       |
|   Dynamic VMAs / mmap | (Base per process: 0x4000000000 + PID * 4GB)
|                       |
+-----------------------+ 0x0000600000000000 (Heap Base / brk)
|   Process Heap Space  | (Expanded via sys_brk)
+-----------------------+
|   ELF Code & Data     | (Loaded from PT_LOAD headers, e.g. 0x400000..)
+-----------------------+ 0x0000000000400000
|                       |
+-----------------------+ 0x00000000FFFFFFFF (4 GB Identity Map Boundary)
|   Direct 1:1 Identity | (Maps Physical RAM: 0x00000000..0xFFFFFFFF)
|   Physical Mapping    | (Used by Kernel Code, GDT, TSS, IDT, Page Tables, MMIO)
+-----------------------+ 0x0000000000000000
```

---

## 🔗 Next Steps & Documentation Guides

For in-depth explanations of individual subsystems, proceed to the subsequent guides:

- [**01 - UEFI Bootloading & Firmware Handoff**](01-uefi-bootloading.md)
- [**02 - Segmentation, GDT, TSS & Exception Architecture**](02-gdt-idt-tss.md)
- [**03 - 4-Level Paging, Frame Allocation & Virtual Memory Manager**](03-paging-and-memory.md)
- [**04 - System Call Subsystem (Linux x86_64 ABI)**](04-syscall-abi.md)
- [**05 - Process Management, Preemptive Scheduling & Synchronization**](05-process-multitasking-sched.md)
- [**06 - In-Memory TarFS Ramdisk & ELF64 Binary Loader**](06-elf-loader-tarfs.md)
- [**07 - FPU & SIMD/SSE Floating-Point Support**](07-fpu-sse.md)
- [**08 - Hardware Device Drivers & Low-Level I/O**](08-hardware-drivers.md)
- [**09 - Userland Runtime, Applications & Concurrency**](09-userland-environment.md)
- [**10 - Specification-Driven Testing & QEMU Automation**](10-testing-and-qemu.md)
- [**11 - Extending BangOS Developer Guide**](11-extending-bangos.md)
- [**12 - Parallel ATA (IDE) Storage Controller & Disk Driver**](12-ata-storage-driver.md)
- [**13 - Virtual File System & ext2 On-Disk Filesystem Engine**](13-vfs-and-ext2.md)
- [**14 - VirtIO-Net Driver & PCI Bus Enumeration**](14-virtio-network-driver.md)
- [**15 - Lightweight In-Kernel TCP/IP Protocol Stack**](15-tcpip-network-stack.md)
- [**16 - POSIX Sockets & Userland HTTP/1.1 Web Client**](16-socket-api-and-http.md)
- [**17 - Build System, Tooling & Developer Workflows**](17-build-system-and-tooling.md)
