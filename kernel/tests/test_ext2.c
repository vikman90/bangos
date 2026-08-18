#include "ktest.h"
#include "fs/ext2/ext2.h"
#include "fs/vfs.h"
#include "lib/kstring.h"

static bool test_ext2_superblock(void) {
    ext2_fs_t *fs = ext2_get_primary_fs();
    KTEST_ASSERT_NOT_NULL(fs);
    KTEST_ASSERT_EQ(fs->sb.s_magic, EXT2_SUPER_MAGIC);
    KTEST_ASSERT(fs->block_size >= 1024);
    KTEST_ASSERT(fs->sb.s_blocks_count > 0);
    return true;
}

static bool test_ext2_file_read(void) {
    vfs_node_t *node = NULL;
    int res = vfs_lookup("/mnt/ext2/welcome.txt", &node);
    KTEST_ASSERT_EQ(res, 0);
    KTEST_ASSERT_NOT_NULL(node);
    KTEST_ASSERT(node->size > 0);

    char buf[128];
    kmemset(buf, 0, sizeof(buf));
    int64_t bytes = node->ops->read(node, 0, 64, buf);
    KTEST_ASSERT(bytes > 0);
    KTEST_ASSERT(kstrstr(buf, "Welcome") != NULL || kstrstr(buf, "BangOS") != NULL || kstrstr(buf, "=") != NULL);

    return true;
}

static bool test_ext2_directory_traversal(void) {
    vfs_node_t *node = NULL;
    int res = vfs_lookup("/mnt/ext2/docs/architecture.txt", &node);
    KTEST_ASSERT_EQ(res, 0);
    KTEST_ASSERT_NOT_NULL(node);
    KTEST_ASSERT(node->size > 0);

    char buf[64];
    kmemset(buf, 0, sizeof(buf));
    int64_t bytes = node->ops->read(node, 0, 32, buf);
    KTEST_ASSERT(bytes > 0);
    KTEST_ASSERT(kstrstr(buf, "BangOS") != NULL || kstrstr(buf, "Storage") != NULL);

    return true;
}

bool test_ext2_all(void) {
    KTEST_RUN(test_ext2_superblock);
    KTEST_RUN(test_ext2_file_read);
    KTEST_RUN(test_ext2_directory_traversal);
    return true;
}
