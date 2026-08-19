#include "ethernet.h"
#include "net.h"
#include "arp.h"
#include "ipv4.h"
#include "drivers/virtio_net.h"
#include "lib/kstring.h"

static uint8_t eth_tx_buffer[2048];

int ethernet_send(const uint8_t *dst_mac, uint16_t ethertype, const void *payload, size_t len) {
    if (!dst_mac || !payload || len == 0 || (ETH_HLEN + len) > sizeof(eth_tx_buffer)) {
        return -1;
    }

    net_config_t *cfg = net_get_config();
    eth_hdr_t *hdr = (eth_hdr_t *)eth_tx_buffer;

    kmemcpy(hdr->dst_mac, dst_mac, ETH_ALEN);
    kmemcpy(hdr->src_mac, cfg->mac, ETH_ALEN);
    hdr->ethertype = htons(ethertype);

    kmemcpy(eth_tx_buffer + ETH_HLEN, payload, len);

    size_t total_len = ETH_HLEN + len;
    // Pad to minimum Ethernet frame size (60 bytes before CRC)
    if (total_len < 60) {
        kmemset(eth_tx_buffer + total_len, 0, 60 - total_len);
        total_len = 60;
    }

    return virtio_net_send_packet(eth_tx_buffer, total_len);
}

void ethernet_receive(const uint8_t *packet, size_t len) {
    if (!packet || len < ETH_HLEN) return;

    const eth_hdr_t *hdr = (const eth_hdr_t *)packet;
    uint16_t ethertype = ntohs(hdr->ethertype);
    const uint8_t *payload = packet + ETH_HLEN;
    size_t payload_len = len - ETH_HLEN;

    if (ethertype == ETHERTYPE_ARP) {
        arp_handle_packet(payload, payload_len);
    } else if (ethertype == ETHERTYPE_IPV4) {
        ipv4_receive(payload, payload_len);
    }
}
