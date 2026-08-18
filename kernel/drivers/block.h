#ifndef DRIVERS_BLOCK_H
#define DRIVERS_BLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLOCK_MAX_DEVICES 8
#define BLOCK_SECTOR_SIZE 512

typedef struct block_dev {
    char name[32];
    uint64_t total_sectors;
    uint32_t sector_size;
    int (*read_blocks)(struct block_dev *dev, uint64_t lba, uint32_t count, void *buffer);
    int (*write_blocks)(struct block_dev *dev, uint64_t lba, uint32_t count, const void *buffer);
    void *priv_data;
} block_dev_t;

int block_register_device(block_dev_t *dev);
block_dev_t *block_get_device(const char *name);
int block_get_device_count(void);
block_dev_t *block_get_device_by_index(int index);

#endif /* DRIVERS_BLOCK_H */
