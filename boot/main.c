#include <efi.h>
#include <efilib.h>
#include "kernel.h"

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    Print(L"[Bootloader] Starting 64-bit UEFI Bootloader for BangOS...\n");

    // 1. Locate file system protocol to load /init ELF
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

    EFI_FILE *AppFile;
    Status = uefi_call_wrapper(RootVolume->Open, 5, RootVolume, &AppFile, L"init", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to open \\init ELF binary\n");
        return Status;
    }

    // 2. Get file size
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, InfoSize, (void **)&FileInfo);
    Status = uefi_call_wrapper(AppFile->GetInfo, 4, AppFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to get FileInfo for \\init\n");
        return Status;
    }

    UINTN ElfSize = FileInfo->FileSize;
    uefi_call_wrapper(BS->FreePool, 1, FileInfo);

    // 3. Allocate memory and read ELF payload
    void *ElfBuffer = NULL;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, ElfSize, &ElfBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to allocate memory for ELF buffer\n");
        return Status;
    }

    Status = uefi_call_wrapper(AppFile->Read, 3, AppFile, &ElfSize, ElfBuffer);
    uefi_call_wrapper(AppFile->Close, 1, AppFile);
    if (EFI_ERROR(Status)) {
        Print(L"[Bootloader Error] Failed to read ELF payload\n");
        return Status;
    }

    Print(L"[Bootloader] Loaded \\init ELF (%d bytes) at 0x%lx\n", ElfSize, (UINT64)ElfBuffer);

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
    boot_info.elf_paddr         = ElfBuffer;
    boot_info.elf_size          = ElfSize;

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
