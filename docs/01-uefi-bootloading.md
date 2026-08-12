# 01 - 64-bit UEFI Bootloader

BangOS uses modern **UEFI** firmware instead of legacy BIOS/MBR. The bootloader is written in C using the 64-bit specification of `gnu-efi` and compiles to an EFI binary application (`BOOTX64.EFI`).

---

## 📂 EFI System Partition (ESP) Layout

UEFI requires a standard FAT32-formatted directory structure:

```
build/esp/
├── EFI/
│   └── BOOT/
│       └── BOOTX64.EFI       # Compiled bootloader binary
└── calc                      # Static userland ELF64 executable
```

---

## ⚙️ Bootloader Execution Steps (`boot/main.c`)

1. **UEFI Library Initialization**:
   ```c
   InitializeLib(ImageHandle, SystemTable);
   ```
2. **Root Volume Opening**:
   Uses `gEfiSimpleFileSystemProtocolGuid` to access the FAT32 disk volume from which the firmware booted.
3. **ELF Executable Loading (`\calc`)**:
   Opens the `calc` file, queries its exact file size via `GetInfo`, allocates memory using `AllocatePool`, and reads the complete ELF payload into RAM.
4. **Physical Memory Map Retrieval**:
   Calls `GetMemoryMap` to retrieve descriptors for available physical memory in the machine.
5. **Exiting Boot Services (`ExitBootServices`)**:
   ```c
   Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
   ```
   From this point forward, UEFI hands complete control of the hardware to the BangOS kernel.
6. **Kernel Entry Jump**:
   Transfers memory map pointers and the ELF payload to `kernel_main(&boot_info)`.
