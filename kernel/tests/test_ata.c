#include "ktest.h"
#include "drivers/ata.h"
#include "drivers/block.h"

static bool test_ata_device_probe(void) {
    KTEST_ASSERT(block_get_device_count() > 0);
    block_dev_t *dev = block_get_device_by_index(0);
    KTEST_ASSERT_NOT_NULL(dev);
    KTEST_ASSERT_EQ(dev->sector_size, 512);
    KTEST_ASSERT(dev->total_sectors > 0);
    return true;
}

static bool test_ata_sector_read(void) {
    block_dev_t *dev = NULL;
    // Find the ext2 device (ata1 or with 0xEF53 magic)
    for (int i = 0; i < block_get_device_count(); i++) {
        block_dev_t *d = block_get_device_by_index(i);
        if (d) {
            uint8_t buffer[512];
            if (d->read_blocks(d, 2, 1, buffer) == 0) {
                uint16_t magic = (uint16_t)buffer[56] | ((uint16_t)buffer[57] << 8);
                if (magic == 0xEF53) {
                    dev = d;
                    break;
                }
            }
        }
    }

    KTEST_ASSERT_NOT_NULL(dev);

    uint8_t buffer[512];
    int res = dev->read_blocks(dev, 0, 1, buffer);
    KTEST_ASSERT_EQ(res, 0);

    // Read sector 2 (offset 1024 bytes -> ext2 Superblock)
    res = dev->read_blocks(dev, 2, 1, buffer);
    KTEST_ASSERT_EQ(res, 0);

    uint16_t magic = (uint16_t)buffer[56] | ((uint16_t)buffer[57] << 8);
    KTEST_ASSERT_EQ(magic, 0xEF53);

    return true;
}

bool test_ata_all(void) {
    KTEST_RUN(test_ata_device_probe);
    KTEST_RUN(test_ata_sector_read);
    return true;
}
