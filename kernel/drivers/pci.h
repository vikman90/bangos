#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define PCI_VENDOR_NONE     0xFFFF

// Common PCI Configuration Registers
#define PCI_REG_VENDOR_ID   0x00
#define PCI_REG_DEVICE_ID   0x02
#define PCI_REG_COMMAND     0x04
#define PCI_REG_STATUS      0x06
#define PCI_REG_REVISION    0x08
#define PCI_REG_PROG_IF     0x09
#define PCI_REG_SUBCLASS    0x0A
#define PCI_REG_CLASS       0x0B
#define PCI_REG_HEADER_TYPE 0x0E
#define PCI_REG_BAR0        0x10
#define PCI_REG_BAR1        0x14
#define PCI_REG_BAR2        0x18
#define PCI_REG_BAR3        0x1C
#define PCI_REG_BAR4        0x20
#define PCI_REG_BAR5        0x24
#define PCI_REG_SUBSYS_VEN  0x2C
#define PCI_REG_SUBSYS_ID   0x2E
#define PCI_REG_IRQ_LINE    0x3C
#define PCI_REG_IRQ_PIN     0x3D

// PCI Command Register Flags
#define PCI_COMMAND_IO      0x0001
#define PCI_COMMAND_MEMORY  0x0002
#define PCI_COMMAND_MASTER  0x0004

#define PCI_MAX_DEVICES     32

typedef struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass_id;
    uint8_t  prog_if;
    uint8_t  revision;
    uint16_t subsys_vendor_id;
    uint16_t subsys_device_id;
    uint32_t bar[6];
    uint8_t  bar_type[6]; // 0 = Memory, 1 = I/O
    uint8_t  irq_line;
    uint8_t  irq_pin;
} pci_device_t;

void     pci_init(void);
uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void     pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
void     pci_write_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_class(uint8_t class_id, uint8_t subclass_id);
int           pci_get_device_count(void);
pci_device_t *pci_get_device(int index);

#endif /* DRIVERS_PCI_H */
