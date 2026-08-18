#ifndef FS_EXT2_H
#define FS_EXT2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs/vfs.h"
#include "drivers/block.h"

#define EXT2_SUPER_MAGIC    0xEF53
#define EXT2_ROOT_INO       2

#define EXT2_S_IFMT         0xF000
#define EXT2_S_IFREG        0x8000
#define EXT2_S_IFDIR        0x4000

#define EXT2_FT_UNKNOWN     0
#define EXT2_FT_REG_FILE    1
#define EXT2_FT_DIR         2
#define EXT2_FT_CHRDEV      3
#define EXT2_FT_BLKDEV      4
#define EXT2_FT_FIFO        5
#define EXT2_FT_SOCK        6
#define EXT2_FT_SYMLINK     7

typedef struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_reserved[190];
} __attribute__((packed)) ext2_super_block_t;

typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed)) ext2_group_desc_t;

typedef struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed)) ext2_inode_t;

typedef struct ext2_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} __attribute__((packed)) ext2_dir_entry_2_t;

typedef struct ext2_fs {
    block_dev_t        *dev;
    ext2_super_block_t  sb;
    uint32_t            block_size;
    uint32_t            sectors_per_block;
    uint32_t            group_count;
    ext2_group_desc_t  *bgds;
    uint32_t            inode_size;
    vfs_node_t          root_node;
} ext2_fs_t;

int ext2_mount_device(block_dev_t *dev, const char *mountpoint, vfs_node_t **out_root);
int ext2_read_inode(ext2_fs_t *fs, uint32_t ino, ext2_inode_t *out_inode);
int ext2_write_inode(ext2_fs_t *fs, uint32_t ino, const ext2_inode_t *in_inode);
int ext2_read_block(ext2_fs_t *fs, uint32_t block_num, void *buffer);
int ext2_write_block(ext2_fs_t *fs, uint32_t block_num, const void *buffer);
uint32_t ext2_alloc_block(ext2_fs_t *fs);
ext2_fs_t *ext2_get_primary_fs(void);

#endif /* FS_EXT2_H */
