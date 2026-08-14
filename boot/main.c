#include <efi.h>
#include <efilib.h>
#include "kernel.h"

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    Print(L"[Bootloader] Starting 64-bit UEFI Bootloader for BangOS...\n");

    // 1. Locate file system protocol to load /initrd.tar
    EFI_LOADED_IMAGE *LoadedImage;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to get LoadedImage protocol\n");
        return Status;
    }

    EFI_FILE_IO_INTERFACE *FileSystem;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to get SimpleFileSystem protocol\n");
        return Status;
    }

    EFI_FILE *RootVolume;
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootVolume);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to open root volume\n");
        return Status;
    }

    EFI_FILE *RamdiskFile;
    Status = uefi_call_wrapper(RootVolume->Open, 5, RootVolume, &RamdiskFile, L"initrd.tar", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Warning] \\initrd.tar not found, trying \\init fallback...\n");
        Status = uefi_call_wrapper(RootVolume->Open, 5, RootVolume, &RamdiskFile, L"init", EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(Status)) {
            Print(L"[Bootloader Error] Failed to open payload file\n");
            return Status;
        }
    }

    // 2. Get file size
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, InfoSize, (void **)&FileInfo);
    Status = uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to get FileInfo for ramdisk\n");
        return Status;
    }

    UINTN RamdiskSize = FileInfo->FileSize;
    uefi_call_wrapper(BS->FreePool, 1, FileInfo);

    // 3. Allocate memory and read ramdisk payload
    void *RamdiskBuffer = NULL;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, RamdiskSize, &RamdiskBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to allocate memory for ramdisk buffer\n");
        return Status;
    }

    Status = uefi_call_wrapper(RamdiskFile->Read, 3, RamdiskFile, &RamdiskSize, RamdiskBuffer);
    uefi_call_wrapper(RamdiskFile->Close, 1, RamdiskFile);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to read ramdisk payload\n");
        return Status;
    }

    Print(L"[Bootloader] Loaded ramdisk (%d bytes) at 0x%lx\n", RamdiskSize, (UINT64)RamdiskBuffer);

    // 4. Get Memory Map
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;

    uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    MemoryMapSize += 2 * DescriptorSize; // Extra padding
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (void **)&MemoryMap);
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);

    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to retrieve memory map\n");
        return Status;
    }

    // 5. Package boot info
    boot_info_t boot_info;
    boot_info.memory_map        = MemoryMap;
    boot_info.memory_map_size   = MemoryMapSize;
    boot_info.descriptor_size   = DescriptorSize;
    boot_info.descriptor_version= DescriptorVersion;
    boot_info.ramdisk_paddr     = RamdiskBuffer;
    boot_info.ramdisk_size      = RamdiskSize;

    // 6. Exit UEFI Boot Services
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        // Retry ExitBootServices if memory map key changed
        uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
        Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    }

    // 7. Jump directly into 64-bit Kernel Main
    kernel_main(&boot_info);

    while (1) {
        __asm__ volatile ("cli; hlt");
    }

    return EFI_SUCCESS;
}
