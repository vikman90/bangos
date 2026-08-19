#ifndef DRIVERS_VIRTIO_NET_H
#define DRIVERS_VIRTIO_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VIRTIO_VENDOR_ID            0x1AF4
#define VIRTIO_DEV_NET_LEGACY       0x1000

// VirtIO Legacy Registers (BAR0 I/O space offsets)
#define VIRTIO_PCI_HOST_FEATURES    0x00
#define VIRTIO_PCI_GUEST_FEATURES   0x04
#define VIRTIO_PCI_QUEUE_PFN        0x08
#define VIRTIO_PCI_QUEUE_NUM        0x0C
#define VIRTIO_PCI_QUEUE_SEL        0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY     0x10
#define VIRTIO_PCI_STATUS           0x12
#define VIRTIO_PCI_ISR              0x13
#define VIRTIO_PCI_CONFIG_OFFSET    0x14

// Device Status Bits
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

// Virtqueue Descriptor Flags
#define VRING_DESC_F_NEXT           1
#define VRING_DESC_F_WRITE          2
#define VRING_DESC_F_INDIRECT       4

// VirtIO Net Features
#define VIRTIO_NET_F_MAC            (1 << 5)
#define VIRTIO_NET_F_STATUS         (1 << 16)

#define VIRTIO_NET_HDR_SIZE         10
#define VIRTIO_NET_MAX_PKT_SIZE     1536
#define VIRTIO_NET_RX_BUFFERS       16

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

typedef struct virtqueue {
    uint16_t queue_index;
    uint16_t queue_size;
    void    *raw_pages;
    uint64_t pfn;
    
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    
    uint16_t last_used_idx;
    uint16_t free_head;
    uint16_t num_free;
} virtqueue_t;

typedef struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct virtio_net_rx_buf {
    virtio_net_hdr_t hdr;
    uint8_t          data[VIRTIO_NET_MAX_PKT_SIZE];
} __attribute__((aligned(16))) virtio_net_rx_buf_t;

typedef struct virtio_net_tx_buf {
    virtio_net_hdr_t hdr;
    uint8_t          data[VIRTIO_NET_MAX_PKT_SIZE];
} __attribute__((aligned(16))) virtio_net_tx_buf_t;

int      virtio_net_init(void);
bool     virtio_net_is_present(void);
void     virtio_net_get_mac(uint8_t *out_mac);
int      virtio_net_send_packet(const void *data, size_t len);
int      virtio_net_poll_packet(void *out_buf, size_t max_len);

#endif /* DRIVERS_VIRTIO_NET_H */
