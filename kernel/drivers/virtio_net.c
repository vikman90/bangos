#include "virtio_net.h"
#include "pci.h"
#include "drivers/uart.h"
#include "mm/memory.h"
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

static bool            net_present = false;
static uint16_t        net_iobase = 0;
static uint8_t         net_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static pci_device_t   *net_pci_dev = NULL;

static virtqueue_t     rx_queue;
static virtqueue_t     tx_queue;

static virtio_net_rx_buf_t *rx_buffers = NULL;
static virtio_net_tx_buf_t *tx_buffer = NULL;

static int init_virtqueue(virtqueue_t *vq, uint16_t queue_index) {
    vq->queue_index = queue_index;

    // Select queue
    outw(net_iobase + VIRTIO_PCI_QUEUE_SEL, queue_index);
    vq->queue_size = inw(net_iobase + VIRTIO_PCI_QUEUE_NUM);

    if (vq->queue_size == 0) {
        kprintf("[VirtIO-Net Error] Queue %u size is 0!\n", queue_index);
        return -1;
    }

    // Cap queue size if overly large to conserve memory
    if (vq->queue_size > 256) {
        vq->queue_size = 256;
    }

    // Calculate memory layout
    uint64_t desc_size = (uint64_t)vq->queue_size * sizeof(struct vring_desc);
    uint64_t avail_size = sizeof(struct vring_avail) + (2 * vq->queue_size);
    uint64_t desc_avail = desc_size + avail_size;
    uint64_t used_offset = (desc_avail + 4095) & ~4095ULL;
    uint64_t used_size = sizeof(struct vring_used) + (sizeof(struct vring_used_elem) * vq->queue_size);
    uint64_t total_size = used_offset + used_size;

    size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    vq->raw_pages = alloc_pages(pages_needed);
    if (!vq->raw_pages) {
        kprintf("[VirtIO-Net Error] Failed to allocate memory for queue %u!\n", queue_index);
        return -1;
    }

    vq->pfn = ((uint64_t)vq->raw_pages) >> 12;
    vq->desc = (struct vring_desc *)vq->raw_pages;
    vq->avail = (struct vring_avail *)((uint8_t *)vq->raw_pages + desc_size);
    vq->used = (struct vring_used *)((uint8_t *)vq->raw_pages + used_offset);

    vq->last_used_idx = 0;
    vq->free_head = 0;
    vq->num_free = vq->queue_size;

    // Tell hardware the page frame number of the virtqueue
    outl(net_iobase + VIRTIO_PCI_QUEUE_PFN, (uint32_t)vq->pfn);

    kprintf("[VirtIO-Net] Queue %u initialized: size=%u, PFN=%p (%lu pages)\n",
            queue_index, vq->queue_size, (void *)vq->pfn, pages_needed);

    return 0;
}

int virtio_net_init(void) {
    kprintf("[VirtIO-Net] Probing for VirtIO Network PCI device...\n");

    net_pci_dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_DEV_NET_LEGACY);
    if (!net_pci_dev) {
        // Search by class (Network controller: class 0x02)
        net_pci_dev = pci_find_class(0x02, 0xFF);
    }

    if (!net_pci_dev) {
        kprintf("[VirtIO-Net Warning] No VirtIO network device detected on PCI bus.\n");
        net_present = false;
        return -1;
    }

    net_iobase = (uint16_t)net_pci_dev->bar[0];
    kprintf("[VirtIO-Net] Device detected at PCI %02x:%02x.%d, I/O Base: 0x%04x\n",
            net_pci_dev->bus, net_pci_dev->slot, net_pci_dev->func, net_iobase);

    // Enable Bus Mastering & I/O space on PCI command register
    uint16_t pci_cmd = pci_read_config_16(net_pci_dev->bus, net_pci_dev->slot, net_pci_dev->func, PCI_REG_COMMAND);
    pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER);
    pci_write_config_16(net_pci_dev->bus, net_pci_dev->slot, net_pci_dev->func, PCI_REG_COMMAND, pci_cmd);

    // 1. Reset device
    outb(net_iobase + VIRTIO_PCI_STATUS, 0);

    // 2. Set ACKNOWLEDGE status bit
    outb(net_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    // 3. Set DRIVER status bit
    outb(net_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // 4. Read host features
    uint32_t host_features = inl(net_iobase + VIRTIO_PCI_HOST_FEATURES);
    kprintf("[VirtIO-Net] Host features: 0x%08x\n", host_features);

    // We accept standard features (including MAC if available)
    uint32_t guest_features = host_features & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS);
    outl(net_iobase + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    // 5. Read MAC address from device configuration space
    if (host_features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < 6; i++) {
            net_mac[i] = inb(net_iobase + VIRTIO_PCI_CONFIG_OFFSET + i);
        }
    }
    kprintf("[VirtIO-Net] Hardware MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
            net_mac[0], net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5]);

    // 6. Initialize Receive Virtqueue (Queue 0)
    if (init_virtqueue(&rx_queue, 0) != 0) {
        kprintf("[VirtIO-Net Error] RX Queue initialization failed!\n");
        return -1;
    }

    // 7. Initialize Transmit Virtqueue (Queue 1)
    if (init_virtqueue(&tx_queue, 1) != 0) {
        kprintf("[VirtIO-Net Error] TX Queue initialization failed!\n");
        return -1;
    }

    // Allocate packet buffers
    size_t rx_pages = (sizeof(virtio_net_rx_buf_t) * VIRTIO_NET_RX_BUFFERS + PAGE_SIZE - 1) / PAGE_SIZE;
    rx_buffers = (virtio_net_rx_buf_t *)alloc_pages(rx_pages);
    tx_buffer = (virtio_net_tx_buf_t *)alloc_pages(1);

    if (!rx_buffers || !tx_buffer) {
        kprintf("[VirtIO-Net Error] Failed to allocate packet buffers!\n");
        return -1;
    }

    kmemset(rx_buffers, 0, rx_pages * PAGE_SIZE);
    kmemset(tx_buffer, 0, PAGE_SIZE);

    // Populate RX queue descriptors
    uint16_t num_rx = (rx_queue.queue_size < VIRTIO_NET_RX_BUFFERS) ? rx_queue.queue_size : VIRTIO_NET_RX_BUFFERS;
    for (uint16_t i = 0; i < num_rx; i++) {
        rx_queue.desc[i].addr = (uint64_t)&rx_buffers[i];
        rx_queue.desc[i].len = sizeof(virtio_net_rx_buf_t);
        rx_queue.desc[i].flags = VRING_DESC_F_WRITE;
        rx_queue.desc[i].next = 0;
        rx_queue.avail->ring[i] = i;
    }
    rx_queue.avail->flags = 0;
    rx_queue.avail->idx = num_rx;

    // Notify device about RX buffers
    outw(net_iobase + VIRTIO_PCI_QUEUE_NOTIFY, 0);

    // 8. Set DRIVER_OK status bit
    outb(net_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    net_present = true;
    kprintf("[VirtIO-Net] Network card initialized & DRIVER_OK (%u RX buffers armed).\n", num_rx);
    return 0;
}

bool virtio_net_is_present(void) {
    return net_present;
}

void virtio_net_get_mac(uint8_t *out_mac) {
    if (out_mac) {
        kmemcpy(out_mac, net_mac, 6);
    }
}

int virtio_net_send_packet(const void *data, size_t len) {
    if (!net_present || !data || len == 0 || len > VIRTIO_NET_MAX_PKT_SIZE) {
        return -1;
    }

    kmemset(&tx_buffer->hdr, 0, sizeof(virtio_net_hdr_t));
    kmemcpy(tx_buffer->data, data, len);

    uint16_t desc_idx = 0;
    tx_queue.desc[desc_idx].addr = (uint64_t)tx_buffer;
    tx_queue.desc[desc_idx].len = (uint32_t)(sizeof(virtio_net_hdr_t) + len);
    tx_queue.desc[desc_idx].flags = 0;
    tx_queue.desc[desc_idx].next = 0;

    uint16_t avail_slot = tx_queue.avail->idx % tx_queue.queue_size;
    tx_queue.avail->ring[avail_slot] = desc_idx;
    
    __asm__ volatile ("" : : : "memory");
    tx_queue.avail->idx++;

    // Notify queue 1 (TX)
    outw(net_iobase + VIRTIO_PCI_QUEUE_NOTIFY, 1);

    // Wait for transmit completion
    uint32_t timeout = 500000;
    while (timeout-- > 0) {
        if (tx_queue.last_used_idx != tx_queue.used->idx) {
            tx_queue.last_used_idx++;
            return (int)len;
        }
        __asm__ volatile ("pause");
    }

    return (int)len; // Completed asynchronously
}

int virtio_net_poll_packet(void *out_buf, size_t max_len) {
    if (!net_present || !out_buf || max_len == 0) {
        return 0;
    }

    if (rx_queue.last_used_idx == rx_queue.used->idx) {
        return 0; // No packets ready
    }

    uint16_t used_slot = rx_queue.last_used_idx % rx_queue.queue_size;
    struct vring_used_elem *elem = &rx_queue.used->ring[used_slot];
    uint32_t desc_id = elem->id;
    uint32_t total_len = elem->len;

    size_t packet_len = 0;
    if (total_len > sizeof(virtio_net_hdr_t)) {
        packet_len = total_len - sizeof(virtio_net_hdr_t);
        if (packet_len > max_len) {
            packet_len = max_len;
        }
        kmemcpy(out_buf, rx_buffers[desc_id].data, packet_len);
    }

    rx_queue.last_used_idx++;

    // Recycle descriptor back into available ring
    uint16_t avail_slot = rx_queue.avail->idx % rx_queue.queue_size;
    rx_queue.avail->ring[avail_slot] = (uint16_t)desc_id;
    __asm__ volatile ("" : : : "memory");
    rx_queue.avail->idx++;

    // Notify RX queue about recycled buffer
    outw(net_iobase + VIRTIO_PCI_QUEUE_NOTIFY, 0);

    return (int)packet_len;
}
