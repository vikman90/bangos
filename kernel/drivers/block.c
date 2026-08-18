#include "block.h"
#include "lib/kstring.h"
#include "drivers/uart.h"

static block_dev_t *registered_devices[BLOCK_MAX_DEVICES];
static int device_count = 0;

int block_register_device(block_dev_t *dev) {
    if (!dev || device_count >= BLOCK_MAX_DEVICES) {
        return -1;
    }
    registered_devices[device_count++] = dev;
    kprintf("[BlockDev] Registered block device '%s' (%u sectors, %u bytes/sector)\n",
            dev->name, (uint32_t)dev->total_sectors, dev->sector_size);
    return 0;
}

block_dev_t *block_get_device(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < device_count; i++) {
        if (kstrcmp(registered_devices[i]->name, name) == 0) {
            return registered_devices[i];
        }
    }
    return NULL;
}

int block_get_device_count(void) {
    return device_count;
}

block_dev_t *block_get_device_by_index(int index) {
    if (index < 0 || index >= device_count) {
        return NULL;
    }
    return registered_devices[index];
}
