# 06 - In-Memory TarFS Ramdisk & ELF64 Binary Loader

BangOS executes userland programs by packaging them into a standard **USTAR (Uniform Standard Tape Archive)** ramdisk (`initrd.tar`) loaded into memory at boot time. The kernel parses the archive in RAM and loads 64-bit ELF binaries directly into virtual memory.

---

## 📦 USTAR Ramdisk Architecture (`kernel/fs/tarfs.c`)

The TAR format consists of sequential 512-byte header blocks followed by file data rounded up to 512-byte block boundaries:

```text
+---------------------+-------------------------------+---------------------+-----+
| Header Block #1     | File Payload (Padded to 512B) | Header Block #2     | ... |
| (512 Bytes)         | e.g. /bin/init ELF binary     | (512 Bytes)         |     |
+---------------------+-------------------------------+---------------------+-----+
```

### USTAR Header Structure (`struct ustar_header`):
```c
struct ustar_header {
    char name[100];     // File path name
    char mode[8];       // File permissions (octal)
    char uid[8];        // User ID (octal)
    char gid[8];        // Group ID (octal)
    char size[12];      // File size in bytes (ASCII octal string)
    char mtime[12];     // Modification time (octal)
    char chksum[8];     // Header checksum
    char typeflag;      // '0' or '\0' for regular file
    char linkname[100]; // Symlink target
    char magic[6];      // "ustar\0"
    char version[2];    // "00"
    char uname[32];     // User name
    char gname[32];     // Group name
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed));
```

### Octal String Parser & Path Matching:
The `size` field is encoded as an ASCII octal string (e.g. `"00000012345 "`):
```c
static uint64_t parse_octal(const char *str, size_t maxlen) {
    uint64_t val = 0;
    for (size_t i = 0; i < maxlen; i++) {
        char c = str[i];
        if (c < '0' || c > '7') break;
        val = (val << 3) | (uint64_t)(c - '0');
    }
    return val;
}
```

Path matching (`tarfs_lookup()`) normalizes leading slashes and matches absolute paths (`/bin/calc`), relative paths (`bin/calc`), and basename searches (`calc`).

---

## ⚡ ELF64 Binary Loader (`kernel/loader/elf.c`)

When `elf_load_binary()` parses an executable image in RAM, it performs validation and memory mapping:

```text
+--------------------------------------------------------------+
| 1. Validate ELF64 Header (Elf64_Ehdr)                        |
|    - Magic: 0x7F 'E' 'L' 'F' (ELF_MAGIC = 0x464C457F)        |
|    - Class: e_ident[4] == 2 (64-Bit Architecture)            |
|    - Target: e_machine == 0x3E (x86_64 Machine)              |
|    - Extract entry point virtual address: ehdr->e_entry      |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 2. Iterate Program Headers (Elf64_Phdr)                      |
|    - Traverse ehdr->e_phnum headers                          |
|    - Filter for headers where p_type == PT_LOAD              |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| 3. Allocate & Populate PT_LOAD Segments                      |
|    - Calculate page-aligned virtual address: vaddr & ~0xFFF  |
|    - Calculate page count: (memsz + offset + 4095) / 4096    |
|    - alloc_pages(num_pages) allocates physical frames        |
|    - kmemcpy() copies filesz bytes of initialized code/data  |
|    - kmemset() clears (memsz - filesz) bytes to zero (BSS)   |
|    - map_user_pages() registers physical frames into paging  |
+--------------------------------------------------------------+
```

---

## 🔍 Detailed Code Walkthrough

```c
int elf_load_binary(const void *elf_data, size_t elf_size, elf_info_t *out_info) {
    if (!elf_data || elf_size < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;

    // Check ELF Magic & Architecture
    if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC ||
        ehdr->e_ident[4] != 2 || ehdr->e_machine != 0x3E) {
        return -1;
    }

    out_info->entry_point = ehdr->e_entry;
    out_info->num_segments = 0;
    const uint8_t *ph_table = (const uint8_t *)elf_data + ehdr->e_phoff;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(ph_table + i * ehdr->e_phentsize);

        if (phdr->p_type == PT_LOAD) {
            uint64_t vaddr = phdr->p_vaddr;
            uint64_t memsz = phdr->p_memsz;
            uint64_t filesz = phdr->p_filesz;
            uint64_t offset = phdr->p_offset;

            uint64_t page_aligned_vaddr = vaddr & ~0xFFFULL;
            uint64_t vaddr_offset = vaddr & 0xFFFULL;
            size_t num_pages = (memsz + vaddr_offset + PAGE_SIZE - 1) / PAGE_SIZE;

            void *phys_pages = alloc_pages(num_pages);
            if (!phys_pages) return -1;

            // Copy initialized code/data
            if (filesz > 0) {
                kmemcpy((uint8_t *)phys_pages + vaddr_offset,
                        (const uint8_t *)elf_data + offset, filesz);
            }
            // Zero-fill uninitialized BSS section
            if (memsz > filesz) {
                kmemset((uint8_t *)phys_pages + vaddr_offset + filesz,
                        0, memsz - filesz);
            }

            // Map into process page table
            map_user_pages(page_aligned_vaddr, (uint64_t)phys_pages, num_pages);

            // Record segment for fork() and context switching
            out_info->segments[out_info->num_segments].virt_addr = page_aligned_vaddr;
            out_info->segments[out_info->num_segments].phys_addr = (uint64_t)phys_pages;
            out_info->segments[out_info->num_segments].num_pages = num_pages;
            out_info->num_segments++;
        }
    }
    return 0;
}
```

By cleanly handling `PT_LOAD` headers, BangOS supports standard static Linux ELF binaries containing separate `.text`, `.rodata`, `.data`, and `.bss` sections generated by standard toolchains.
