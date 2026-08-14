#include "tarfs.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed));

static const uint8_t *tar_archive_base = NULL;
static size_t tar_archive_size = 0;

static uint64_t parse_octal(const char *str, size_t maxlen) {
    uint64_t val = 0;
    for (size_t i = 0; i < maxlen; i++) {
        char c = str[i];
        if (c < '0' || c > '7') {
            break;
        }
        val = (val << 3) | (uint64_t)(c - '0');
    }
    return val;
}

static const char *normalize_path(const char *path) {
    if (!path) return "";
    while (*path == '/' || *path == '.') {
        path++;
    }
    return path;
}

static bool path_matches(const char *target, const char *entry_name) {
    const char *norm_target = normalize_path(target);
    const char *norm_entry = normalize_path(entry_name);

    if (kstrcmp(norm_target, norm_entry) == 0) {
        return true;
    }

    // Also match if target is "calc" and entry is "bin/calc"
    if (kstrncmp(norm_entry, "bin/", 4) == 0 && kstrcmp(norm_target, norm_entry + 4) == 0) {
        return true;
    }
    if (kstrncmp(norm_target, "bin/", 4) == 0 && kstrcmp(norm_target + 4, norm_entry) == 0) {
        return true;
    }

    return false;
}

void tarfs_init(const void *tar_base, size_t tar_size) {
    tar_archive_base = (const uint8_t *)tar_base;
    tar_archive_size = tar_size;

    kprintf("[TarFS] Initialized in-memory initrd at %p (%u bytes)\n",
            tar_base, (uint32_t)tar_size);
}

void tarfs_list_files(void) {
    if (!tar_archive_base || tar_archive_size < 512) {
        kprintf("[TarFS] No ramdisk loaded.\n");
        return;
    }

    kprintf("[TarFS] Listing ramdisk contents:\n");
    size_t offset = 0;
    while (offset + 512 <= tar_archive_size) {
        const struct ustar_header *hdr = (const struct ustar_header *)(tar_archive_base + offset);
        if (hdr->name[0] == '\0') {
            break; // End of TAR archive
        }

        uint64_t file_size = parse_octal(hdr->size, sizeof(hdr->size));
        kprintf("  - %s (type: %c, size: %u bytes)\n", hdr->name, hdr->typeflag ? hdr->typeflag : '0', (uint32_t)file_size);

        offset += 512 + ((file_size + 511) & ~511ULL);
    }
}

int tarfs_lookup(const char *path, const void **out_data, size_t *out_size) {
    if (!tar_archive_base || !path || tar_archive_size < 512) {
        return -1;
    }

    size_t offset = 0;
    while (offset + 512 <= tar_archive_size) {
        const struct ustar_header *hdr = (const struct ustar_header *)(tar_archive_base + offset);
        if (hdr->name[0] == '\0') {
            break;
        }

        uint64_t file_size = parse_octal(hdr->size, sizeof(hdr->size));

        if (hdr->typeflag == '0' || hdr->typeflag == '\0') {
            if (path_matches(path, hdr->name)) {
                if (out_data) *out_data = (const void *)(tar_archive_base + offset + 512);
                if (out_size) *out_size = (size_t)file_size;
                return 0;
            }
        }

        offset += 512 + ((file_size + 511) & ~511ULL);
    }

    return -1;
}
