#include "ktest.h"
#include "fs/tarfs.h"

static bool test_tarfs_lookup_invalid_args(void) {
    const void *data = NULL;
    size_t size = 0;

    KTEST_ASSERT_EQ(tarfs_lookup(NULL, &data, &size), -1);
    KTEST_ASSERT_EQ(tarfs_lookup("/bin/nonexistent_file_xyz_123", &data, &size), -1);
    return true;
}

static bool test_tarfs_lookup_path_variations(void) {
    const void *d1 = NULL, *d2 = NULL, *d3 = NULL;
    size_t s1 = 0, s2 = 0, s3 = 0;

    int r1 = tarfs_lookup("init", &d1, &s1);
    int r2 = tarfs_lookup("bin/init", &d2, &s2);
    int r3 = tarfs_lookup("/bin/init", &d3, &s3);

    KTEST_ASSERT_EQ(r1, 0);
    KTEST_ASSERT_EQ(r2, 0);
    KTEST_ASSERT_EQ(r3, 0);

    KTEST_ASSERT_NOT_NULL(d1);
    KTEST_ASSERT_EQ(d1, d2);
    KTEST_ASSERT_EQ(d2, d3);
    KTEST_ASSERT_EQ(s1, s2);
    KTEST_ASSERT(s1 > 0);

    return true;
}

bool test_tarfs_all(void) {
    kprintf("\n--- [KTEST] Running Ramdisk (TarFS) Tests ---\n");
    KTEST_RUN(test_tarfs_lookup_invalid_args);
    KTEST_RUN(test_tarfs_lookup_path_variations);
    return true;
}
