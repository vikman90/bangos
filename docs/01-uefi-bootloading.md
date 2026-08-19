# 01 - 64-bit UEFI Bootloading & Firmware Handoff

BangOS boots via modern **UEFI (Unified Extensible Firmware Interface)** rather than legacy 16-bit BIOS / MBR. The bootloader is implemented in 64-bit C using `gnu-efi` and compiles to a standard PE32+ executable placed at `\EFI\BOOT\BOOTX64.EFI` on the FAT32 EFI System Partition (ESP).

---

## 📂 EFI System Partition (ESP) Layout

```text
build/esp/
├── EFI/
│   └── BOOT/
│       └── BOOTX64.EFI       # 64-bit UEFI Bootloader application
└── initrd.tar                # USTAR ramdisk containing standalone userland ELFs
```

---

## ⚙️ Bootloader Execution Flow (`boot/main.c`)

The bootloader executes in 64-bit Long Mode within the UEFI firmware environment before the kernel takes direct control of the CPU:

```text
+--------------------------------------------------------------+
| 1. efi_main(ImageHandle, SystemTable)                        |
|    - Initializes GNU-EFI library helper structures           |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 2. Locate FAT32 File System Protocol                         |
|    - Queries gEfiLoadedImageProtocolGuid on ImageHandle      |
|    - Queries gEfiSimpleFileSystemProtocolGuid on DeviceHandle|
|    - Opens root filesystem volume (OpenVolume)               |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 3. Read Ramdisk Payload (\initrd.tar) into Memory            |
|    - Opens "initrd.tar" (with fallback to "init")            |
|    - Queries exact file size using GetInfo(EFI_FILE_INFO)    |
|    - Allocates physical memory pool via AllocatePool()       |
|    - Reads entire archive into RamdiskBuffer                 |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 4. Fetch UEFI Physical Memory Map                            |
|    - Queries GetMemoryMap() to obtain descriptors & MapKey   |
|    - Allocates descriptor buffer with safety padding         |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 5. ExitBootServices & Handoff to BangOS Kernel               |
|    - Calls ExitBootServices(ImageHandle, MapKey)             |
|    - If MapKey changed during allocation, refreshes & retries|
|    - Calls kernel_main(&boot_info)                           |
+--------------------------------------------------------------+
```

---

## 🔍 Detailed Code Walkthrough

### 1. Accessing the Boot Filesystem Volume
The bootloader first queries the handle protocol on its own image to determine which device volume it was launched from:

```c
EFI_LOADED_IMAGE *LoadedImage;
Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
                           &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);

EFI_FILE_IO_INTERFACE *FileSystem;
Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle,
                           &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);

EFI_FILE *RootVolume;
Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootVolume);
```

### 2. Loading the `initrd.tar` Ramdisk
The bootloader attempts to open `initrd.tar`. Once opened, it queries its size via `GetInfo` using `gEfiFileInfoGuid`, allocates physical memory with `AllocatePool(EfiLoaderData)`, and reads the complete archive into memory:

```c
EFI_FILE *RamdiskFile;
Status = uefi_call_wrapper(RootVolume->Open, 5, RootVolume, &RamdiskFile,
                           L"initrd.tar", EFI_FILE_MODE_READ, 0);

EFI_FILE_INFO *FileInfo;
UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, InfoSize, (void **)&FileInfo);
uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);

UINTN RamdiskSize = FileInfo->FileSize;
uefi_call_wrapper(BS->FreePool, 1, FileInfo);

void *RamdiskBuffer = NULL;
uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, RamdiskSize, &RamdiskBuffer);
uefi_call_wrapper(RamdiskFile->Read, 3, RamdiskFile, &RamdiskSize, RamdiskBuffer);
uefi_call_wrapper(RamdiskFile->Close, 1, RamdiskFile);
```

### 3. Retrieving the Physical Memory Map
UEFI maintains a list of memory regions (conventional RAM, ACPI tables, MMIO, firmware reserved). BangOS queries this map to understand the available physical address space:

```c
UINTN MemoryMapSize = 0;
EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
UINTN MapKey = 0;
UINTN DescriptorSize = 0;
UINT32 DescriptorVersion = 0;

uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey,
                  &DescriptorSize, &DescriptorVersion);
MemoryMapSize += 2 * DescriptorSize; // Padding for subsequent allocation
uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (void **)&MemoryMap);
uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey,
                  &DescriptorSize, &DescriptorVersion);
```

### 4. Constructing the `boot_info_t` Struct
All boot parameters needed by the kernel are packaged into a clean C structure defined in [`include/kernel.h`](file:///root/test/little-bang/include/kernel.h):

```c
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
```

### 5. Terminating UEFI Boot Services (`ExitBootServices`)
Calling `ExitBootServices` is the critical point of no return:

- It terminates firmware boot services, freeing firmware-allocated scratch memory.
- It disables UEFI timer interrupts and hands direct ownership of the hardware to BangOS.

> [!IMPORTANT]
> **MapKey Synchronization**: Calling `AllocatePool()` modifies the memory map and increments `MapKey`. If `ExitBootServices` fails because the key changed, the bootloader re-fetches the map and key immediately before retrying `ExitBootServices`.

```c
Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
if (EFI_ERROR(Status)) {
    // Refresh memory map key and retry
    uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey,
                      &DescriptorSize, &DescriptorVersion);
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
}

// Transfer execution directly into 64-bit Kernel Main
kernel_main(&boot_info);
```

---

## 🛡️ CPU State Upon Kernel Entry

When `kernel_main()` begins execution:

1. **Paging**: The CPU is running in 64-bit Long Mode using the UEFI firmware's identity-mapped page tables.
2. **Interrupts**: Masked / Disabled (`cli`).
3. **Segments**: Firmware default segments are active (BangOS immediately initializes its own GDT).
4. **Parameters**: Register `RDI` holds the pointer to `boot_info_t` (as per System V AMD64 ABI).
