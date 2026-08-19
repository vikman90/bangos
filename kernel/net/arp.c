#include "arp.h"
#include "ethernet.h"
#include "net.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

#define ARP_CACHE_SIZE 16

typedef struct arp_entry {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static const uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void arp_init(void) {
    kmemset(arp_cache, 0, sizeof(arp_cache));

    // Pre-populate QEMU SLIRP default gateway MAC (52:55:0a:00:02:02)
    uint8_t qemu_gw_mac[6] = { 0x52, 0x55, 0x0A, 0x00, 0x02, 0x02 };
    arp_insert_cache(DEFAULT_GATEWAY_IP, qemu_gw_mac);
}

void arp_insert_cache(uint32_t ip, const uint8_t *mac) {
    if (!mac) return;

    // Check if IP already in cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            kmemcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }

    // Insert into first free slot
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            kmemcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = true;
            return;
        }
    }

    // Evict slot 0 if full
    arp_cache[0].ip = ip;
    kmemcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = true;
}

void arp_request(uint32_t target_ip) {
    net_config_t *cfg = net_get_config();
    arp_hdr_t arp;
    kmemset(&arp, 0, sizeof(arp));

    arp.htype = htons(ARP_HTYPE_ETHERNET);
    arp.ptype = htons(ARP_PTYPE_IPV4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.opcode = htons(ARP_OP_REQUEST);

    kmemcpy(arp.src_mac, cfg->mac, 6);
    arp.src_ip = cfg->local_ip;
    kmemset(arp.dst_mac, 0, 6);
    arp.dst_ip = target_ip;

    ethernet_send(broadcast_mac, ETHERTYPE_ARP, &arp, sizeof(arp));
}

void arp_handle_packet(const uint8_t *payload, size_t len) {
    if (!payload || len < sizeof(arp_hdr_t)) return;

    const arp_hdr_t *arp = (const arp_hdr_t *)payload;
    if (ntohs(arp->htype) != ARP_HTYPE_ETHERNET || ntohs(arp->ptype) != ARP_PTYPE_IPV4) {
        return;
    }

    net_config_t *cfg = net_get_config();

    // Cache the sender's IP and MAC
    arp_insert_cache(arp->src_ip, arp->src_mac);

    uint16_t op = ntohs(arp->opcode);
    if (op == ARP_OP_REQUEST && arp->dst_ip == cfg->local_ip) {
        // Send ARP Reply
        arp_hdr_t reply;
        reply.htype = htons(ARP_HTYPE_ETHERNET);
        reply.ptype = htons(ARP_PTYPE_IPV4);
        reply.hlen = 6;
        reply.plen = 4;
        reply.opcode = htons(ARP_OP_REPLY);

        kmemcpy(reply.src_mac, cfg->mac, 6);
        reply.src_ip = cfg->local_ip;
        kmemcpy(reply.dst_mac, arp->src_mac, 6);
        reply.dst_ip = arp->src_ip;

        ethernet_send(arp->src_mac, ETHERTYPE_ARP, &reply, sizeof(reply));
    }
}

int arp_resolve(uint32_t target_ip, uint8_t *out_mac) {
    if (!out_mac) return -1;

    if (target_ip == 0xFFFFFFFF) {
        kmemcpy(out_mac, broadcast_mac, 6);
        return 0;
    }

    // Check cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == target_ip) {
            kmemcpy(out_mac, arp_cache[i].mac, 6);
            return 0;
        }
    }

    // Send ARP request and poll for reply
    arp_request(target_ip);

    for (int poll_iter = 0; poll_iter < 1000; poll_iter++) {
        net_poll();
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arp_cache[i].valid && arp_cache[i].ip == target_ip) {
                kmemcpy(out_mac, arp_cache[i].mac, 6);
                return 0;
            }
        }
        __asm__ volatile ("pause");
    }

    // If resolving gateway, use QEMU SLIRP default as fallback
    net_config_t *cfg = net_get_config();
    if (target_ip == cfg->gateway_ip) {
        uint8_t qemu_gw_mac[6] = { 0x52, 0x55, 0x0A, 0x00, 0x02, 0x02 };
        kmemcpy(out_mac, qemu_gw_mac, 6);
        return 0;
    }

    return -1;
}
