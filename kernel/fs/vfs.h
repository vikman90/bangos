#ifndef FS_VFS_H
#define FS_VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "drivers/block.h"

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEV     0x03
#define VFS_BLOCKDEV    0x04

#define VFS_MAX_MOUNTS  8
#define VFS_MAX_FD      32

#define VFS_O_RDONLY    0x0000
#define VFS_O_WRONLY    0x0001
#define VFS_O_RDWR      0x0002
#define VFS_O_CREAT     0x0040
#define VFS_O_EXCL      0x0080
#define VFS_O_TRUNC     0x0200
#define VFS_O_APPEND    0x0400

#define VFS_SEEK_SET    0
#define VFS_SEEK_CUR    1
#define VFS_SEEK_END    2

struct vfs_node;

typedef struct vfs_dirent {
    uint32_t d_ino;
    char     d_name[128];
    uint8_t  d_type;
} vfs_dirent_t;

typedef struct vfs_ops {
    int (*open)(struct vfs_node *node, int flags);
    int (*close)(struct vfs_node *node);
    int64_t (*read)(struct vfs_node *node, uint64_t offset, size_t size, void *buffer);
    int64_t (*write)(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer);
    int (*readdir)(struct vfs_node *node, uint32_t index, vfs_dirent_t *out_dirent);
    int (*finddir)(struct vfs_node *node, const char *name, struct vfs_node **out_child);
    int (*create)(struct vfs_node *dir, const char *name, int mode, struct vfs_node **out_child);
} vfs_ops_t;

typedef struct vfs_node {
    char        name[128];
    uint32_t    type;
    uint32_t    inode;
    uint64_t    size;
    uint32_t    permissions;
    uint32_t    flags;
    int         refcount;
    vfs_ops_t  *ops;
    void       *priv_data;
} vfs_node_t;

typedef struct vfs_mount {
    char        mountpoint[64];
    vfs_node_t *root_node;
    const char *fs_type;
    block_dev_t *dev;
} vfs_mount_t;

typedef struct file_desc {
    vfs_node_t *node;
    uint64_t    offset;
    int         flags;
    bool        in_use;
} file_desc_t;

void vfs_init(void);
int vfs_mount(const char *mountpoint, const char *fs_type, block_dev_t *dev, vfs_node_t *root_node);
vfs_mount_t *vfs_find_mount(const char *path, const char **out_relpath);
int vfs_lookup(const char *path, vfs_node_t **out_node);
int vfs_open(const char *path, int flags, int mode, vfs_node_t **out_node);

// Process-level file descriptor APIs
void fd_table_init(file_desc_t *table);
int fd_alloc(file_desc_t *table, vfs_node_t *node, int flags);
int fd_free(file_desc_t *table, int fd);
file_desc_t *fd_get(file_desc_t *table, int fd);
void fd_table_clone(file_desc_t *dst, const file_desc_t *src);

#endif /* FS_VFS_H */
