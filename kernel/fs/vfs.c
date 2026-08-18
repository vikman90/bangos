#include "vfs.h"
#include "lib/kstring.h"
#include "drivers/uart.h"
#include "mm/memory.h"

static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static int mount_count = 0;

void vfs_init(void) {
    kmemset(mounts, 0, sizeof(mounts));
    mount_count = 0;
    kprintf("[VFS] Virtual File System initialized.\n");
}

int vfs_mount(const char *mountpoint, const char *fs_type, block_dev_t *dev, vfs_node_t *root_node) {
    if (!mountpoint || !root_node || mount_count >= VFS_MAX_MOUNTS) {
        return -1;
    }

    vfs_mount_t *m = &mounts[mount_count++];
    kstrncpy(m->mountpoint, mountpoint, sizeof(m->mountpoint));
    m->fs_type = fs_type;
    m->dev = dev;
    m->root_node = root_node;

    kprintf("[VFS] Mounted '%s' (%s) at '%s'\n",
            dev ? dev->name : "memory", fs_type ? fs_type : "generic", mountpoint);
    return 0;
}

vfs_mount_t *vfs_find_mount(const char *path, const char **out_relpath) {
    if (!path) return NULL;

    vfs_mount_t *best_mount = NULL;
    size_t best_len = 0;

    for (int i = 0; i < mount_count; i++) {
        size_t mlen = kstrlen(mounts[i].mountpoint);
        if (kstrncmp(path, mounts[i].mountpoint, mlen) == 0) {
            if (path[mlen] == '/' || path[mlen] == '\0' || mlen == 1) {
                if (mlen >= best_len) {
                    best_len = mlen;
                    best_mount = &mounts[i];
                }
            }
        }
    }

    if (best_mount && out_relpath) {
        const char *rel = path + best_len;
        while (*rel == '/') rel++;
        *out_relpath = rel;
    }

    return best_mount;
}

int vfs_lookup(const char *path, vfs_node_t **out_node) {
    if (!path || !out_node) return -1;

    const char *relpath = NULL;
    vfs_mount_t *mount = vfs_find_mount(path, &relpath);
    if (!mount || !mount->root_node) {
        return -1;
    }

    if (!relpath || *relpath == '\0') {
        *out_node = mount->root_node;
        return 0;
    }

    vfs_node_t *curr = mount->root_node;
    char comp[128];
    const char *p = relpath;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        size_t i = 0;
        while (*p && *p != '/' && i < sizeof(comp) - 1) {
            comp[i++] = *p++;
        }
        comp[i] = '\0';

        if (!curr->ops || !curr->ops->finddir) {
            return -1;
        }

        vfs_node_t *next = NULL;
        int res = curr->ops->finddir(curr, comp, &next);
        if (res != 0 || !next) {
            return -1; // Path component not found
        }
        curr = next;
    }

    *out_node = curr;
    return 0;
}

int vfs_open(const char *path, int flags, int mode, vfs_node_t **out_node) {
    if (!path || !out_node) return -1;
    vfs_node_t *node = NULL;
    int res = vfs_lookup(path, &node);
    if (res == 0 && node) {
        if (node->ops && node->ops->open) {
            if (node->ops->open(node, flags) != 0) {
                return -1;
            }
        }
        node->refcount++;
        *out_node = node;
        return 0;
    }

    // If file does not exist and O_CREAT requested:
    if (flags & (VFS_O_CREAT | 0x40)) {
        char dirpath[128];
        const char *last_slash = kstrrchr(path, '/');
        const char *filename = NULL;

        if (last_slash) {
            size_t dirlen = (size_t)(last_slash - path);
            if (dirlen == 0) dirlen = 1;
            kstrncpy(dirpath, path, dirlen + 1);
            filename = last_slash + 1;
        } else {
            kstrncpy(dirpath, "/", sizeof(dirpath));
            filename = path;
        }

        vfs_node_t *parent_dir = NULL;
        if (vfs_lookup(dirpath, &parent_dir) == 0 && parent_dir) {
            if (parent_dir->ops && parent_dir->ops->create) {
                vfs_node_t *new_node = NULL;
                if (parent_dir->ops->create(parent_dir, filename, mode, &new_node) == 0 && new_node) {
                    new_node->refcount++;
                    *out_node = new_node;
                    return 0;
                }
            }
        }
    }

    return -1;
}

void fd_table_init(file_desc_t *table) {
    if (!table) return;
    for (int i = 0; i < VFS_MAX_FD; i++) {
        table[i].node = NULL;
        table[i].offset = 0;
        table[i].flags = 0;
        table[i].in_use = false;
    }
}

int fd_alloc(file_desc_t *table, vfs_node_t *node, int flags) {
    if (!table || !node) return -1;

    // FDs 0, 1, 2 reserved for stdio
    for (int i = 3; i < VFS_MAX_FD; i++) {
        if (!table[i].in_use) {
            table[i].node = node;
            table[i].offset = (flags & VFS_O_APPEND) ? node->size : 0;
            table[i].flags = flags;
            table[i].in_use = true;
            return i;
        }
    }
    return -1; // Out of file descriptors
}

int fd_free(file_desc_t *table, int fd) {
    if (!table || fd < 3 || fd >= VFS_MAX_FD || !table[fd].in_use) {
        return -1;
    }

    if (table[fd].node) {
        if (table[fd].node->ops && table[fd].node->ops->close) {
            table[fd].node->ops->close(table[fd].node);
        }
        if (table[fd].node->refcount > 0) {
            table[fd].node->refcount--;
        }
    }

    table[fd].node = NULL;
    table[fd].offset = 0;
    table[fd].flags = 0;
    table[fd].in_use = false;
    return 0;
}

file_desc_t *fd_get(file_desc_t *table, int fd) {
    if (!table || fd < 0 || fd >= VFS_MAX_FD || !table[fd].in_use) {
        return NULL;
    }
    return &table[fd];
}

void fd_table_clone(file_desc_t *dst, const file_desc_t *src) {
    if (!dst || !src) return;
    for (int i = 0; i < VFS_MAX_FD; i++) {
        dst[i] = src[i];
        if (dst[i].in_use && dst[i].node) {
            dst[i].node->refcount++;
        }
    }
}
