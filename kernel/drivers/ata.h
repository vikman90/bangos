#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "block.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_FEATURES    1
#define ATA_REG_SEC_COUNT   2
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRIVE       6
#define ATA_REG_STATUS      7
#define ATA_REG_COMMAND     7

#define ATA_STATUS_ERR      0x01
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_DF       0x20
#define ATA_STATUS_DRDY     0x40
#define ATA_STATUS_BSY      0x80

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_FLUSH       0xE7
#define ATA_CMD_FLUSH_EXT   0xEA
#define ATA_CMD_IDENTIFY    0xEC

typedef struct ata_drive {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t  drive_num; // 0 = Master (0xA0), 1 = Slave (0xB0)
    bool     exists;
    bool     lba48_supported;
    uint64_t sector_count;
    char     model[41];
    char     serial[21];
    block_dev_t block_dev;
} ata_drive_t;

void ata_init(void);
int ata_read_sectors(ata_drive_t *drive, uint64_t lba, uint32_t count, void *buf);
int ata_write_sectors(ata_drive_t *drive, uint64_t lba, uint32_t count, const void *buf);
ata_drive_t *ata_get_primary_drive(void);

#endif /* DRIVERS_ATA_H */
