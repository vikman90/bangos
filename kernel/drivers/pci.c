#include "pci.h"
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

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static pci_device_t pci_device_list[PCI_MAX_DEVICES];
static int pci_device_count = 0;

static uint32_t pci_config_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((1U << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)slot << 11) |
                      ((uint32_t)func << 8) |
                      (offset & 0xFC));
}

uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return inw(PCI_CONFIG_DATA + (offset & 2));
}

uint8_t pci_read_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return inb(PCI_CONFIG_DATA + (offset & 3));
}

void pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outw(PCI_CONFIG_DATA + (offset & 2), val);
}

void pci_write_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outb(PCI_CONFIG_DATA + (offset & 3), val);
}

static void pci_scan_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_read_config_16(bus, slot, func, PCI_REG_VENDOR_ID);
    if (vendor_id == PCI_VENDOR_NONE || vendor_id == 0) {
        return;
    }

    if (pci_device_count >= PCI_MAX_DEVICES) {
        return;
    }

    pci_device_t *dev = &pci_device_list[pci_device_count];
    kmemset(dev, 0, sizeof(pci_device_t));

    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_read_config_16(bus, slot, func, PCI_REG_DEVICE_ID);
    dev->revision = pci_read_config_8(bus, slot, func, PCI_REG_REVISION);
    dev->prog_if = pci_read_config_8(bus, slot, func, PCI_REG_PROG_IF);
    dev->subclass_id = pci_read_config_8(bus, slot, func, PCI_REG_SUBCLASS);
    dev->class_id = pci_read_config_8(bus, slot, func, PCI_REG_CLASS);
    dev->subsys_vendor_id = pci_read_config_16(bus, slot, func, PCI_REG_SUBSYS_VEN);
    dev->subsys_device_id = pci_read_config_16(bus, slot, func, PCI_REG_SUBSYS_ID);
    dev->irq_line = pci_read_config_8(bus, slot, func, PCI_REG_IRQ_LINE);
    dev->irq_pin = pci_read_config_8(bus, slot, func, PCI_REG_IRQ_PIN);

    // Read BARs
    for (int b = 0; b < 6; b++) {
        uint32_t bar_val = pci_read_config_32(bus, slot, func, PCI_REG_BAR0 + (b * 4));
        if (bar_val & 1) {
            // I/O space
            dev->bar[b] = bar_val & ~0x3;
            dev->bar_type[b] = 1;
        } else {
            // Memory space
            dev->bar[b] = bar_val & ~0xF;
            dev->bar_type[b] = 0;
        }
    }

    kprintf("[PCI] Found %02x:%02x.%d: Vendor=%04x Device=%04x Class=%02x.%02x (BAR0=%p %s, IRQ=%u)\n",
            bus, slot, func, dev->vendor_id, dev->device_id, dev->class_id, dev->subclass_id,
            (void *)(uint64_t)dev->bar[0], dev->bar_type[0] ? "I/O" : "MMIO", dev->irq_line);

    pci_device_count++;
}

void pci_init(void) {
    pci_device_count = 0;
    kmemset(pci_device_list, 0, sizeof(pci_device_list));

    kprintf("[PCI] Scanning PCI bus hierarchy...\n");

    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_config_16((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (vendor == PCI_VENDOR_NONE || vendor == 0) {
                continue;
            }

            pci_scan_device((uint8_t)bus, slot, 0);

            uint8_t header_type = pci_read_config_8((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            if (header_type & 0x80) {
                // Multi-function device
                for (uint8_t func = 1; func < 8; func++) {
                    pci_scan_device((uint8_t)bus, slot, func);
                }
            }
        }
    }

    kprintf("[PCI] Enumeration complete. Total devices detected: %d\n", pci_device_count);
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_device_list[i].vendor_id == vendor_id &&
            (device_id == 0xFFFF || pci_device_list[i].device_id == device_id)) {
            return &pci_device_list[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_id, uint8_t subclass_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_device_list[i].class_id == class_id &&
            (subclass_id == 0xFF || pci_device_list[i].subclass_id == subclass_id)) {
            return &pci_device_list[i];
        }
    }
    return NULL;
}

int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t *pci_get_device(int index) {
    if (index >= 0 && index < pci_device_count) {
        return &pci_device_list[index];
    }
    return NULL;
}
