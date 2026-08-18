#include "ext2.h"
#include "drivers/uart.h"
#include "lib/kstring.h"
#include "mm/memory.h"

static ext2_fs_t primary_ext2;
static bool primary_ext2_initialized = false;

int ext2_read_block(ext2_fs_t *fs, uint32_t block_num, void *buffer) {
    if (!fs || !fs->dev || !buffer) return -1;
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return fs->dev->read_blocks(fs->dev, lba, fs->sectors_per_block, buffer);
}

int ext2_write_block(ext2_fs_t *fs, uint32_t block_num, const void *buffer) {
    if (!fs || !fs->dev || !buffer) return -1;
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return fs->dev->write_blocks(fs->dev, lba, fs->sectors_per_block, buffer);
}

int ext2_read_inode(ext2_fs_t *fs, uint32_t ino, ext2_inode_t *out_inode) {
    if (!fs || !out_inode || ino == 0) return -1;

    uint32_t bg = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t idx = (ino - 1) % fs->sb.s_inodes_per_group;
    if (bg >= fs->group_count) return -1;

    uint32_t inode_table_block = fs->bgds[bg].bg_inode_table;
    uint32_t block_offset = (idx * fs->inode_size) / fs->block_size;
    uint32_t byte_offset = (idx * fs->inode_size) % fs->block_size;

    static uint8_t blk_buf[4096];
    if (ext2_read_block(fs, inode_table_block + block_offset, blk_buf) != 0) {
        return -1;
    }

    kmemcpy(out_inode, blk_buf + byte_offset, sizeof(ext2_inode_t));
    return 0;
}

int ext2_write_inode(ext2_fs_t *fs, uint32_t ino, const ext2_inode_t *in_inode) {
    if (!fs || !in_inode || ino == 0) return -1;

    uint32_t bg = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t idx = (ino - 1) % fs->sb.s_inodes_per_group;
    if (bg >= fs->group_count) return -1;

    uint32_t inode_table_block = fs->bgds[bg].bg_inode_table;
    uint32_t block_offset = (idx * fs->inode_size) / fs->block_size;
    uint32_t byte_offset = (idx * fs->inode_size) % fs->block_size;

    static uint8_t blk_buf[4096];
    if (ext2_read_block(fs, inode_table_block + block_offset, blk_buf) != 0) {
        return -1;
    }

    kmemcpy(blk_buf + byte_offset, in_inode, sizeof(ext2_inode_t));
    return ext2_write_block(fs, inode_table_block + block_offset, blk_buf);
}

uint32_t ext2_alloc_block(ext2_fs_t *fs) {
    if (!fs) return 0;

    static uint8_t bitmap[4096];
    for (uint32_t bg = 0; bg < fs->group_count; bg++) {
        if (fs->bgds[bg].bg_free_blocks_count == 0) continue;

        uint32_t b_bitmap_block = fs->bgds[bg].bg_block_bitmap;
        if (ext2_read_block(fs, b_bitmap_block, bitmap) != 0) continue;

        for (uint32_t b = 0; b < fs->sb.s_blocks_per_group; b++) {
            uint32_t byte_idx = b / 8;
            uint32_t bit_idx = b % 8;
            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                bitmap[byte_idx] |= (1 << bit_idx);
                ext2_write_block(fs, b_bitmap_block, bitmap);

                fs->bgds[bg].bg_free_blocks_count--;
                fs->sb.s_free_blocks_count--;

                uint32_t allocated_block = bg * fs->sb.s_blocks_per_group + b + fs->sb.s_first_data_block;
                return allocated_block;
            }
        }
    }
    return 0; // Disk full
}

uint32_t ext2_alloc_inode(ext2_fs_t *fs) {
    if (!fs) return 0;

    static uint8_t bitmap[4096];
    for (uint32_t bg = 0; bg < fs->group_count; bg++) {
        if (fs->bgds[bg].bg_free_inodes_count == 0) continue;

        uint32_t i_bitmap_block = fs->bgds[bg].bg_inode_bitmap;
        if (ext2_read_block(fs, i_bitmap_block, bitmap) != 0) continue;

        for (uint32_t i = 0; i < fs->sb.s_inodes_per_group; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;
            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                bitmap[byte_idx] |= (1 << bit_idx);
                ext2_write_block(fs, i_bitmap_block, bitmap);

                fs->bgds[bg].bg_free_inodes_count--;
                fs->sb.s_free_inodes_count--;

                uint32_t allocated_inode = bg * fs->sb.s_inodes_per_group + i + 1;
                return allocated_inode;
            }
        }
    }
    return 0; // No free inodes
}

static uint32_t ext2_get_file_block(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t file_block_num) {
    if (file_block_num < 12) {
        return inode->i_block[file_block_num];
    }

    uint32_t ptrs_per_block = fs->block_size / 4;
    file_block_num -= 12;

    if (file_block_num < ptrs_per_block) {
        // Single indirect block
        uint32_t indirect_block = inode->i_block[12];
        if (indirect_block == 0) return 0;

        static uint32_t indirect_buf[1024];
        if (ext2_read_block(fs, indirect_block, indirect_buf) != 0) return 0;

        return indirect_buf[file_block_num];
    }

    file_block_num -= ptrs_per_block;
    uint32_t double_limit = ptrs_per_block * ptrs_per_block;
    if (file_block_num < double_limit) {
        // Double indirect block
        uint32_t dbl_indirect_block = inode->i_block[13];
        if (dbl_indirect_block == 0) return 0;

        static uint32_t dbl_buf[1024];
        if (ext2_read_block(fs, dbl_indirect_block, dbl_buf) != 0) return 0;

        uint32_t primary_idx = file_block_num / ptrs_per_block;
        uint32_t secondary_idx = file_block_num % ptrs_per_block;

        uint32_t sec_block = dbl_buf[primary_idx];
        if (sec_block == 0) return 0;

        static uint32_t sec_buf[1024];
        if (ext2_read_block(fs, sec_block, sec_buf) != 0) return 0;

        return sec_buf[secondary_idx];
    }

    return 0;
}

static int64_t ext2_vfs_read(vfs_node_t *node, uint64_t offset, size_t size, void *buffer);
static int64_t ext2_vfs_write(vfs_node_t *node, uint64_t offset, size_t size, const void *buffer);
static int ext2_vfs_finddir(vfs_node_t *dir, const char *name, vfs_node_t **out_child);
static int ext2_vfs_readdir(vfs_node_t *dir, uint32_t index, vfs_dirent_t *out_dirent);
static int ext2_vfs_create(vfs_node_t *dir, const char *name, int mode, vfs_node_t **out_child);

static vfs_ops_t ext2_dir_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .readdir = ext2_vfs_readdir,
    .finddir = ext2_vfs_finddir,
    .create = ext2_vfs_create
};

static vfs_ops_t ext2_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = ext2_vfs_read,
    .write = ext2_vfs_write,
    .readdir = NULL,
    .finddir = NULL,
    .create = NULL
};

typedef struct ext2_node_info {
    ext2_fs_t   *fs;
    uint32_t     ino;
    ext2_inode_t inode;
} ext2_node_info_t;

static ext2_node_info_t node_pool[128];
static vfs_node_t vfs_node_pool[128];
static int pool_count = 0;

static vfs_node_t *ext2_create_vfs_node(ext2_fs_t *fs, uint32_t ino, const char *name, const ext2_inode_t *in) {
    if (pool_count >= 128) return NULL;

    // Dynamically assign function pointers to ensure correct runtime relocation
    ext2_dir_ops.readdir = ext2_vfs_readdir;
    ext2_dir_ops.finddir = ext2_vfs_finddir;
    ext2_dir_ops.create = ext2_vfs_create;
    ext2_file_ops.read = ext2_vfs_read;
    ext2_file_ops.write = ext2_vfs_write;

    int idx = pool_count++;
    ext2_node_info_t *info = &node_pool[idx];
    vfs_node_t *vnode = &vfs_node_pool[idx];

    info->fs = fs;
    info->ino = ino;
    info->inode = *in;

    kmemset(vnode, 0, sizeof(vfs_node_t));
    kstrncpy(vnode->name, name ? name : "", sizeof(vnode->name));
    vnode->inode = ino;
    vnode->size = in->i_size;
    vnode->permissions = in->i_mode & 0777;
    vnode->priv_data = info;

    if ((in->i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
        vnode->type = VFS_DIRECTORY;
        vnode->ops = &ext2_dir_ops;
    } else {
        vnode->type = VFS_FILE;
        vnode->ops = &ext2_file_ops;
    }

    return vnode;
}

static int64_t ext2_vfs_read(vfs_node_t *node, uint64_t offset, size_t size, void *buffer) {
    if (!node || !buffer || !node->priv_data) return -1;
    ext2_node_info_t *info = (ext2_node_info_t *)node->priv_data;
    ext2_fs_t *fs = info->fs;

    if (offset >= info->inode.i_size) return 0;

    size_t to_read = size;
    if (offset + to_read > info->inode.i_size) {
        to_read = info->inode.i_size - offset;
    }

    uint8_t read_blk_buf[1024];
    size_t bytes_read = 0;
    uint8_t *out = (uint8_t *)buffer;

    while (bytes_read < to_read) {
        uint64_t cur_pos = offset + bytes_read;
        uint32_t file_blk = (uint32_t)(cur_pos / fs->block_size);
        uint32_t blk_offset = (uint32_t)(cur_pos % fs->block_size);
        size_t chunk = fs->block_size - blk_offset;
        if (chunk > to_read - bytes_read) {
            chunk = to_read - bytes_read;
        }

        uint32_t phys_blk = ext2_get_file_block(fs, &info->inode, file_blk);
        if (phys_blk == 0) {
            kmemset(out + bytes_read, 0, chunk); // Sparse file hole
        } else {
            if (ext2_read_block(fs, phys_blk, read_blk_buf) != 0) {
                break;
            }
            kmemcpy(out + bytes_read, read_blk_buf + blk_offset, chunk);
        }

        bytes_read += chunk;
    }

    return (int64_t)bytes_read;
}

static int64_t ext2_vfs_write(vfs_node_t *node, uint64_t offset, size_t size, const void *buffer) {
    if (!node || !buffer || !node->priv_data) return -1;
    ext2_node_info_t *info = (ext2_node_info_t *)node->priv_data;
    ext2_fs_t *fs = info->fs;

    static uint8_t blk_buf[4096];
    size_t bytes_written = 0;
    const uint8_t *in = (const uint8_t *)buffer;

    while (bytes_written < size) {
        uint64_t cur_pos = offset + bytes_written;
        uint32_t file_blk = (uint32_t)(cur_pos / fs->block_size);
        uint32_t blk_offset = (uint32_t)(cur_pos % fs->block_size);
        size_t chunk = fs->block_size - blk_offset;
        if (chunk > size - bytes_written) {
            chunk = size - bytes_written;
        }

        if (file_blk < 12) {
            if (info->inode.i_block[file_blk] == 0) {
                uint32_t new_blk = ext2_alloc_block(fs);
                if (new_blk == 0) break; // Out of space
                info->inode.i_block[file_blk] = new_blk;
                info->inode.i_blocks += (fs->block_size / 512);
                kmemset(blk_buf, 0, fs->block_size);
            } else {
                ext2_read_block(fs, info->inode.i_block[file_blk], blk_buf);
            }

            kmemcpy(blk_buf + blk_offset, in + bytes_written, chunk);
            ext2_write_block(fs, info->inode.i_block[file_blk], blk_buf);
        } else {
            // For simplicity in Phase 1, write supports direct blocks (up to 12 KB per file)
            break;
        }

        bytes_written += chunk;
    }

    if (offset + bytes_written > info->inode.i_size) {
        info->inode.i_size = (uint32_t)(offset + bytes_written);
        node->size = info->inode.i_size;
        ext2_write_inode(fs, info->ino, &info->inode);
    }

    return (int64_t)bytes_written;
}

static int ext2_vfs_finddir(vfs_node_t *dir, const char *name, vfs_node_t **out_child) {
    if (!dir || !name || !out_child || !dir->priv_data) return -1;
    ext2_node_info_t *info = (ext2_node_info_t *)dir->priv_data;
    ext2_fs_t *fs = info->fs;

    static uint8_t s_find_buf[1024];
    uint32_t num_blocks = (info->inode.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0) num_blocks = 1;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t phys_blk = ext2_get_file_block(fs, &info->inode, b);
        if (phys_blk == 0) continue;

        if (ext2_read_block(fs, phys_blk, s_find_buf) != 0) {
            continue;
        }

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            ext2_dir_entry_2_t *entry = (ext2_dir_entry_2_t *)(s_find_buf + offset);
            if (entry->rec_len == 0) {
                break;
            }

            if (entry->inode != 0 && entry->name_len > 0) {
                if (kstrlen(name) == entry->name_len &&
                    kstrncmp(name, entry->name, entry->name_len) == 0) {
                    ext2_inode_t child_inode;
                    if (ext2_read_inode(fs, entry->inode, &child_inode) == 0) {
                        char clean_name[128];
                        size_t nlen = entry->name_len < 127 ? entry->name_len : 127;
                        kmemcpy(clean_name, entry->name, nlen);
                        clean_name[nlen] = '\0';

                        *out_child = ext2_create_vfs_node(fs, entry->inode, clean_name, &child_inode);
                        return *out_child ? 0 : -1;
                    }
                }
            }

            offset += entry->rec_len;
        }
    }

    return -1; // Entry not found
}

static int ext2_vfs_readdir(vfs_node_t *dir, uint32_t index, vfs_dirent_t *out_dirent) {
    if (!dir || !out_dirent || !dir->priv_data) return -1;
    ext2_node_info_t *info = (ext2_node_info_t *)dir->priv_data;
    ext2_fs_t *fs = info->fs;

    uint8_t rdir_buf[1024];
    uint32_t num_blocks = (info->inode.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0) num_blocks = 1;
    uint32_t cur_idx = 0;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t phys_blk = ext2_get_file_block(fs, &info->inode, b);
        if (phys_blk == 0) continue;

        if (ext2_read_block(fs, phys_blk, rdir_buf) != 0) continue;

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            ext2_dir_entry_2_t *entry = (ext2_dir_entry_2_t *)(rdir_buf + offset);
            if (entry->rec_len == 0) break;

            if (entry->inode != 0 && entry->name_len > 0) {
                if (cur_idx == index) {
                    out_dirent->d_ino = entry->inode;
                    out_dirent->d_type = (entry->file_type == EXT2_FT_DIR) ? VFS_DIRECTORY : VFS_FILE;
                    size_t nlen = entry->name_len < sizeof(out_dirent->d_name) - 1 ? entry->name_len : sizeof(out_dirent->d_name) - 1;
                    kmemcpy(out_dirent->d_name, entry->name, nlen);
                    out_dirent->d_name[nlen] = '\0';
                    return 0;
                }
                cur_idx++;
            }

            offset += entry->rec_len;
        }
    }

    return -1;
}

static int ext2_vfs_create(vfs_node_t *dir, const char *name, int mode, vfs_node_t **out_child) {
    if (!dir || !name || !out_child || !dir->priv_data) return -1;
    ext2_node_info_t *info = (ext2_node_info_t *)dir->priv_data;
    ext2_fs_t *fs = info->fs;

    uint32_t new_ino = ext2_alloc_inode(fs);
    if (new_ino == 0) return -1;

    ext2_inode_t new_inode;
    kmemset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.i_mode = EXT2_S_IFREG | (mode ? (mode & 0777) : 0644);
    new_inode.i_size = 0;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0;
    ext2_write_inode(fs, new_ino, &new_inode);

    // Add directory entry into dir
    static uint8_t dir_blk[1024];
    uint32_t dir_phys_blk = ext2_get_file_block(fs, &info->inode, 0);
    if (dir_phys_blk == 0) {
        dir_phys_blk = ext2_alloc_block(fs);
        info->inode.i_block[0] = dir_phys_blk;
        info->inode.i_blocks += (fs->block_size / 512);
        info->inode.i_size = fs->block_size;
        kmemset(dir_blk, 0, fs->block_size);
        ext2_write_inode(fs, info->ino, &info->inode);
    } else {
        ext2_read_block(fs, dir_phys_blk, dir_blk);
    }

    // Traverse to the last directory entry and split rec_len
    size_t name_len = kstrlen(name);
    uint32_t needed_len = 8 + ((name_len + 3) & ~3);
    uint32_t offset = 0;

    while (offset < fs->block_size) {
        ext2_dir_entry_2_t *entry = (ext2_dir_entry_2_t *)(dir_blk + offset);
        if (entry->rec_len == 0) break;

        uint32_t actual_used_len = 8 + ((entry->name_len + 3) & ~3);
        if (entry->rec_len >= actual_used_len + needed_len) {
            uint16_t leftover = entry->rec_len - (uint16_t)actual_used_len;
            entry->rec_len = (uint16_t)actual_used_len;

            ext2_dir_entry_2_t *new_entry = (ext2_dir_entry_2_t *)(dir_blk + offset + actual_used_len);
            new_entry->inode = new_ino;
            new_entry->rec_len = leftover;
            new_entry->name_len = (uint8_t)name_len;
            new_entry->file_type = EXT2_FT_REG_FILE;
            kmemcpy(new_entry->name, name, name_len);

            ext2_write_block(fs, dir_phys_blk, dir_blk);
            break;
        }

        offset += entry->rec_len;
    }

    *out_child = ext2_create_vfs_node(fs, new_ino, name, &new_inode);
    return *out_child ? 0 : -1;
}

int ext2_mount_device(block_dev_t *dev, const char *mountpoint, vfs_node_t **out_root) {
    if (!dev || !mountpoint) return -1;

    ext2_fs_t *fs = &primary_ext2;
    fs->dev = dev;

    // Read Superblock (LBA 2 = byte 1024)
    uint8_t sb_buf[1024];
    if (dev->read_blocks(dev, 2, 2, sb_buf) != 0) {
        kprintf("[ext2 Error] Failed to read Superblock from '%s'\n", dev->name);
        return -1;
    }

    kmemcpy(&fs->sb, sb_buf, sizeof(ext2_super_block_t));

    if (fs->sb.s_magic != EXT2_SUPER_MAGIC) {
        kprintf("[ext2 Error] Invalid magic 0x%x (expected 0xEF53) on '%s'\n",
                fs->sb.s_magic, dev->name);
        return -1;
    }

    fs->block_size = 1024 << fs->sb.s_log_block_size;
    fs->sectors_per_block = fs->block_size / dev->sector_size;
    fs->inode_size = fs->sb.s_rev_level >= 1 ? fs->sb.s_inode_size : 128;
    if (fs->inode_size == 0) fs->inode_size = 128;

    fs->group_count = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;

    // Read Block Group Descriptors (located right after superblock block)
    uint32_t bgd_block = (fs->block_size == 1024) ? 2 : 1;
    static ext2_group_desc_t bgds_buf[64];
    if (ext2_read_block(fs, bgd_block, bgds_buf) != 0) {
        kprintf("[ext2 Error] Failed to read Block Group Descriptors from '%s'\n", dev->name);
        return -1;
    }
    fs->bgds = bgds_buf;

    // Read Root Inode (Inode 2)
    ext2_inode_t root_inode;
    if (ext2_read_inode(fs, EXT2_ROOT_INO, &root_inode) != 0) {
        kprintf("[ext2 Error] Failed to read Root Inode (2) from '%s'\n", dev->name);
        return -1;
    }

    vfs_node_t *root_vnode = ext2_create_vfs_node(fs, EXT2_ROOT_INO, mountpoint, &root_inode);
    if (!root_vnode) {
        return -1;
    }

    if (vfs_mount(mountpoint, "ext2", dev, root_vnode) != 0) {
        return -1;
    }

    if (out_root) *out_root = root_vnode;
    primary_ext2_initialized = true;

    kprintf("[ext2] Mounted ext2 volume: BlockSize=%u, TotalBlocks=%u, FreeBlocks=%u, TotalInodes=%u\n",
            fs->block_size, fs->sb.s_blocks_count, fs->sb.s_free_blocks_count, fs->sb.s_inodes_count);

    return 0;
}

ext2_fs_t *ext2_get_primary_fs(void) {
    return primary_ext2_initialized ? &primary_ext2 : NULL;
}
