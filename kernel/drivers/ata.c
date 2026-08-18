#include "ata.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void ata_delay(uint16_t ctrl_base) {
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
}

static int ata_wait_ready(uint16_t io_base, uint16_t ctrl_base) {
    ata_delay(ctrl_base);
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return 0;
        }
    }
    return -1; // Timeout
}

static int ata_wait_drq(uint16_t io_base, uint16_t ctrl_base) {
    ata_delay(ctrl_base);
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return -1; // Hardware error
        }
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return 0; // Data ready
        }
    }
    return -1; // Timeout
}

static ata_drive_t drives[4];
static int total_drives = 0;

static int ata_dev_read_blocks(block_dev_t *dev, uint64_t lba, uint32_t count, void *buf) {
    ata_drive_t *drive = (ata_drive_t *)dev->priv_data;
    if (!drive) return -1;
    return ata_read_sectors(drive, lba, count, buf);
}

static int ata_dev_write_blocks(block_dev_t *dev, uint64_t lba, uint32_t count, const void *buf) {
    ata_drive_t *drive = (ata_drive_t *)dev->priv_data;
    if (!drive) return -1;
    return ata_write_sectors(drive, lba, count, buf);
}

static void ata_identify_drive(uint16_t io_base, uint16_t ctrl_base, uint8_t drive_num, int index) {
    ata_drive_t *drive = &drives[index];
    drive->io_base = io_base;
    drive->ctrl_base = ctrl_base;
    drive->drive_num = drive_num;
    drive->exists = false;

    // Select drive
    outb(io_base + ATA_REG_DRIVE, 0xA0 | (drive_num << 4));
    ata_delay(ctrl_base);

    // Clear sector count & LBA registers
    outb(io_base + ATA_REG_SEC_COUNT, 0);
    outb(io_base + ATA_REG_LBA_LO, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HI, 0);

    // Send IDENTIFY command
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ctrl_base);

    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        return; // No drive present
    }

    // Wait until BSY clears
    if (ata_wait_ready(io_base, ctrl_base) != 0) {
        return;
    }

    // Check if device is ATAPI or non-ATA
    uint8_t lba_mid = inb(io_base + ATA_REG_LBA_MID);
    uint8_t lba_hi = inb(io_base + ATA_REG_LBA_HI);
    if ((lba_mid == 0x14 && lba_hi == 0xEB) || (lba_mid == 0x69 && lba_hi == 0x96)) {
        return; // ATAPI device (e.g. CD-ROM)
    }

    if (ata_wait_drq(io_base, ctrl_base) != 0) {
        return;
    }

    // Read 256 words (512 bytes) of identification data
    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(io_base + ATA_REG_DATA);
    }

    drive->exists = true;

    // Parse model string (words 27..46)
    for (int i = 0; i < 20; i++) {
        uint16_t w = identify_buf[27 + i];
        drive->model[i * 2] = (char)(w >> 8);
        drive->model[i * 2 + 1] = (char)(w & 0xFF);
    }
    drive->model[40] = '\0';
    // Trim trailing spaces
    for (int i = 39; i >= 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0';
    }

    // Parse serial number (words 10..19)
    for (int i = 0; i < 10; i++) {
        uint16_t w = identify_buf[10 + i];
        drive->serial[i * 2] = (char)(w >> 8);
        drive->serial[i * 2 + 1] = (char)(w & 0xFF);
    }
    drive->serial[20] = '\0';
    for (int i = 19; i >= 0 && drive->serial[i] == ' '; i--) {
        drive->serial[i] = '\0';
    }

    // Check 48-bit LBA support (word 83, bit 10)
    if (identify_buf[83] & (1 << 10)) {
        drive->lba48_supported = true;
        drive->sector_count = ((uint64_t)identify_buf[103] << 48) |
                              ((uint64_t)identify_buf[102] << 32) |
                              ((uint64_t)identify_buf[101] << 16) |
                              ((uint64_t)identify_buf[100]);
    } else {
        drive->lba48_supported = false;
        drive->sector_count = ((uint32_t)identify_buf[61] << 16) | identify_buf[60];
    }

    // Register with block device layer
    char dev_name[32];
    kstrncpy(dev_name, "ata", sizeof(dev_name));
    dev_name[3] = (char)('0' + total_drives);
    dev_name[4] = '\0';

    kstrncpy(drive->block_dev.name, dev_name, sizeof(drive->block_dev.name));
    drive->block_dev.total_sectors = drive->sector_count;
    drive->block_dev.sector_size = BLOCK_SECTOR_SIZE;
    drive->block_dev.read_blocks = ata_dev_read_blocks;
    drive->block_dev.write_blocks = ata_dev_write_blocks;
    drive->block_dev.priv_data = drive;

    block_register_device(&drive->block_dev);
    total_drives++;

    uint32_t size_mb = (uint32_t)((drive->sector_count * BLOCK_SECTOR_SIZE) / (1024 * 1024));
    kprintf("[ATA] Detected Drive '%s': Model='%s', Capacity=%u MB (%u sectors, %s)\n",
            drive->block_dev.name, drive->model, size_mb, (uint32_t)drive->sector_count,
            drive->lba48_supported ? "LBA48" : "LBA28");
}

void ata_init(void) {
    kprintf("[ATA] Probing IDE/ATA storage controllers...\n");
    total_drives = 0;

    // Primary Channel
    ata_identify_drive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0, 0); // Primary Master
    ata_identify_drive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 1, 1); // Primary Slave

    // Secondary Channel
    ata_identify_drive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 0, 2); // Secondary Master
    ata_identify_drive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 1, 3); // Secondary Slave

    kprintf("[ATA] Probing complete. %d ATA storage drive(s) online.\n", total_drives);
}

int ata_read_sectors(ata_drive_t *drive, uint64_t lba, uint32_t count, void *buf) {
    if (!drive || !drive->exists || !buf || count == 0) {
        return -1;
    }
    if (lba + count > drive->sector_count) {
        return -1;
    }

    uint16_t *ptr = (uint16_t *)buf;

    for (uint32_t s = 0; s < count; s++) {
        uint64_t cur_lba = lba + s;

        if (ata_wait_ready(drive->io_base, drive->ctrl_base) != 0) {
            return -1;
        }

        if (cur_lba > 0x0FFFFFFFULL) {
            // LBA48 PIO Read
            outb(drive->io_base + ATA_REG_DRIVE, 0x40 | (drive->drive_num << 4));
            ata_delay(drive->ctrl_base);
            outb(drive->io_base + ATA_REG_SEC_COUNT, 0);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)(cur_lba >> 24));
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 32));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 40));

            outb(drive->io_base + ATA_REG_SEC_COUNT, 1);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)cur_lba);
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 8));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 16));

            outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
        } else {
            // LBA28 PIO Read
            outb(drive->io_base + ATA_REG_DRIVE, 0xE0 | (drive->drive_num << 4) | ((cur_lba >> 24) & 0x0F));
            ata_delay(drive->ctrl_base);
            outb(drive->io_base + ATA_REG_SEC_COUNT, 1);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)cur_lba);
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 8));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 16));

            outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
        }

        if (ata_wait_drq(drive->io_base, drive->ctrl_base) != 0) {
            return -1;
        }

        // Read 256 16-bit words (512 bytes)
        for (int i = 0; i < 256; i++) {
            *ptr++ = inw(drive->io_base + ATA_REG_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(ata_drive_t *drive, uint64_t lba, uint32_t count, const void *buf) {
    if (!drive || !drive->exists || !buf || count == 0) {
        return -1;
    }
    if (lba + count > drive->sector_count) {
        return -1;
    }

    const uint16_t *ptr = (const uint16_t *)buf;

    for (uint32_t s = 0; s < count; s++) {
        uint64_t cur_lba = lba + s;

        if (ata_wait_ready(drive->io_base, drive->ctrl_base) != 0) {
            return -1;
        }

        if (cur_lba > 0x0FFFFFFFULL) {
            // LBA48 PIO Write
            outb(drive->io_base + ATA_REG_DRIVE, 0x40 | (drive->drive_num << 4));
            ata_delay(drive->ctrl_base);
            outb(drive->io_base + ATA_REG_SEC_COUNT, 0);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)(cur_lba >> 24));
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 32));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 40));

            outb(drive->io_base + ATA_REG_SEC_COUNT, 1);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)cur_lba);
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 8));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 16));

            outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
        } else {
            // LBA28 PIO Write
            outb(drive->io_base + ATA_REG_DRIVE, 0xE0 | (drive->drive_num << 4) | ((cur_lba >> 24) & 0x0F));
            ata_delay(drive->ctrl_base);
            outb(drive->io_base + ATA_REG_SEC_COUNT, 1);
            outb(drive->io_base + ATA_REG_LBA_LO, (uint8_t)cur_lba);
            outb(drive->io_base + ATA_REG_LBA_MID, (uint8_t)(cur_lba >> 8));
            outb(drive->io_base + ATA_REG_LBA_HI, (uint8_t)(cur_lba >> 16));

            outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
        }

        if (ata_wait_drq(drive->io_base, drive->ctrl_base) != 0) {
            return -1;
        }

        // Write 256 16-bit words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(drive->io_base + ATA_REG_DATA, *ptr++);
        }

        // Flush Cache
        outb(drive->io_base + ATA_REG_COMMAND, (cur_lba > 0x0FFFFFFFULL) ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
        ata_wait_ready(drive->io_base, drive->ctrl_base);
    }

    return 0;
}

ata_drive_t *ata_get_primary_drive(void) {
    for (int i = 0; i < 4; i++) {
        if (drives[i].exists) {
            return &drives[i];
        }
    }
    return NULL;
}
