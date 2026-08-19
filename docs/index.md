# BangOS Documentation 💥

Welcome to the technical documentation for **BangOS**, an educational, lightweight 64-bit bare-metal operating system microkernel designed to run standard Linux binaries directly on x86_64 UEFI hardware without the Linux kernel.

---

## 🌟 Core Highlights

- **Firmware & Bootloading**: 64-bit GNU-EFI bootloader (`BOOTX64.EFI`), FAT32 file system traversal, GOP graphics, and physical memory map discovery.
- **Microkernel Architecture**: Long mode segmentation (GDT/TSS), 256-vector interrupt handling (IDT with `IST1`), 4-level paging (PML4 to PT) with zero-fill demand paging (`#PF`), and FPU/SSE SIMD state preservation.
- **Multitasking & Concurrency**: 100 Hz 8254 PIT timer preemption, Round-Robin scheduler, `fork()`, `clone()` multithreading with `CLONE_VM`, and fast userspace locking via `futex`.
- **Linux ABI & Syscall Trampoline**: High-performance hardware `syscall` / `sysret` handling adhering to the POSIX / Linux x86_64 ABI.
- **In-Memory TarFS Ramdisk**: Initial ramdisk (`initrd.tar`) parsed from RAM loading standalone 64-bit ELF executables statically linked with `musl-gcc`.
- **Persistent Storage & ext2 Filesystem**: Parallel ATA (IDE) PIO driver and fully-featured Linux Second Extended Filesystem (`ext2`) mounted under the VFS at `/mnt/ext2`.
- **VirtIO-Net & In-Kernel TCP/IP Stack**: Legacy OASIS PCI VirtIO network controller with Virtqueues, Ethernet II, ARP, IPv4 routing, ICMP ping, UDP, RFC 1035 DNS, TCP state machine, and POSIX socket system calls.

---

## 📚 Table of Contents

| Chapter | Title | Summary |
| :--- | :--- | :--- |
| [**00. Architecture Overview**](00-architecture.md) | System Architecture & Design Philosophy | High-level topology, Ring 0 vs Ring 3 privilege separation, and memory map layout. |
| [**01. UEFI Bootloading**](01-uefi-bootloading.md) | 64-bit UEFI Bootloading & Firmware Handoff | Bootloader stages, file loading, EFI memory maps, and transition to kernel mode. |
| [**02. GDT, TSS & IDT**](02-gdt-idt-tss.md) | Segmentation, GDT, TSS & Exception Handling | Segment descriptors, user privilege transitions, interrupt gates, and PIC remapping. |
| [**03. Paging & Memory Management**](03-paging-and-memory.md) | 4-Level Paging, Allocators & Demand Paging | PML4 paging hierarchy, bitmap frame allocator, heap management, and `#PF` demand paging. |
| [**04. System Call ABI**](04-syscall-abi.md) | System Call Subsystem (Linux x86_64 ABI) | MSR configuration, register passing conventions, and implemented POSIX syscalls. |
| [**05. Process Management & Scheduling**](05-process-multitasking-sched.md) | Process Management, Preemptive Sched & Futex | Process Control Blocks (`process_t`), thread spawning, Round-Robin scheduling, and futex. |
| [**06. In-Memory TarFS & ELF64 Loader**](06-elf-loader-tarfs.md) | In-Memory TarFS Ramdisk & ELF64 Loader | USTAR archive parsing, ELF64 header validation, and `PT_LOAD` virtual memory mapping. |
| [**07. FPU & SSE Extensions**](07-fpu-sse.md) | FPU & SIMD/SSE Floating-Point Support | CR0/CR4 configuration, `fxsave64`/`fxrstor64` context switching, and SIMD execution. |
| [**08. Hardware Device Drivers**](08-hardware-drivers.md) | Hardware Device Drivers & Low-Level I/O | 16550 UART serial driver, 8254 PIT timer, PS/2 keyboard, and QEMU power control. |
| [**09. Userland Environment**](09-userland-environment.md) | Userland Runtime, Applications & Concurrency | Static `musl-gcc` compilation, `/bin/init` supervisor, standalone ELFs, and TUI library. |
| [**10. Specification Testing & QEMU**](10-testing-and-qemu.md) | Specification-Driven Testing & QEMU Harness | Kernel unit tests (`ktest`), userland POSIX spec runners, and headless QEMU serial automation. |
| [**11. Extending BangOS**](11-extending-bangos.md) | Extending BangOS Developer Guide | Tutorial on adding new system calls, device drivers, userland tools, and automated tests. |
| [**12. ATA Storage Controller**](12-ata-storage-driver.md) | Parallel ATA (IDE) Storage Controller & Disk Driver | PIO mode 28-bit/48-bit LBA disk block I/O, sector caching, and partition management. |
| [**13. VFS & ext2 Filesystem**](13-vfs-and-ext2.md) | Virtual File System & ext2 On-Disk Filesystem Engine | Inode indexing, directory tables, block groups, file persistence, and multi-mount VFS. |
| [**14. VirtIO Network Driver**](14-virtio-network-driver.md) | VirtIO-Net Driver & PCI Bus Enumeration | OASIS legacy VirtIO network interface, PCI BAR access, split Virtqueues, and packet rings. |
| [**15. TCP/IP Protocol Stack**](15-tcpip-network-stack.md) | Lightweight In-Kernel TCP/IP Protocol Stack | Ethernet II, ARP dynamic cache, IPv4 routing, checksum mathematics, ICMP ping, UDP, and TCP FSM. |
| [**16. Sockets & HTTP Client**](16-socket-api-and-http.md) | POSIX Sockets & Userland HTTP/1.1 Web Client | BSD/POSIX socket system calls, `/bin/netfetch` diagnostic tool, and TLS/HTTPS integration roadmap. |
| [**17. Build System & Tooling**](17-build-system-and-tooling.md) | Build System, Tooling & Developer Workflows | Makefile targets, QEMU interactive mode, GDB debugging, Docker containers, and MkDocs. |
