#include "ipv4.h"
#include "net.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "lib/kstring.h"

static uint16_t ip_id_counter = 1;
static uint8_t  ipv4_tx_buffer[2048];

int ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload, size_t len) {
    if (!payload || len == 0 || (sizeof(ipv4_hdr_t) + len) > sizeof(ipv4_tx_buffer)) {
        return -1;
    }

    net_config_t *cfg = net_get_config();
    ipv4_hdr_t *hdr = (ipv4_hdr_t *)ipv4_tx_buffer;

    hdr->version_ihl = 0x45; // IPv4, IHL = 5 (20 bytes)
    hdr->tos = 0;
    hdr->total_length = htons((uint16_t)(sizeof(ipv4_hdr_t) + len));
    hdr->id = htons(ip_id_counter++);
    hdr->flags_fragment = htons(0x4000); // DF (Don't Fragment)
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = cfg->local_ip;
    hdr->dst_ip = dst_ip;

    hdr->checksum = net_checksum(hdr, sizeof(ipv4_hdr_t));

    kmemcpy(ipv4_tx_buffer + sizeof(ipv4_hdr_t), payload, len);

    // Routing decision: check if destination is on the local subnet
    uint32_t next_hop_ip;
    if ((dst_ip & cfg->netmask) == (cfg->local_ip & cfg->netmask)) {
        next_hop_ip = dst_ip;
    } else {
        next_hop_ip = cfg->gateway_ip;
    }

    uint8_t dst_mac[6];
    if (arp_resolve(next_hop_ip, dst_mac) != 0) {
        return -1; // Failed to resolve MAC
    }

    return ethernet_send(dst_mac, ETHERTYPE_IPV4, ipv4_tx_buffer, sizeof(ipv4_hdr_t) + len);
}

void ipv4_receive(const uint8_t *payload, size_t len) {
    if (!payload || len < sizeof(ipv4_hdr_t)) return;

    const ipv4_hdr_t *hdr = (const ipv4_hdr_t *)payload;
    if ((hdr->version_ihl >> 4) != 4) return; // Not IPv4

    size_t ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(ipv4_hdr_t) || ihl > len) return;

    // Checksum validation
    if (net_checksum(hdr, ihl) != 0) {
        return; // Checksum corrupt
    }

    net_config_t *cfg = net_get_config();
    if (hdr->dst_ip != cfg->local_ip && hdr->dst_ip != 0xFFFFFFFF) {
        return; // Not addressed to us
    }

    const uint8_t *proto_payload = payload + ihl;
    size_t proto_len = len - ihl;

    if (hdr->protocol == IP_PROTO_ICMP) {
        icmp_receive(hdr->src_ip, proto_payload, proto_len);
    } else if (hdr->protocol == IP_PROTO_UDP) {
        udp_receive(hdr->src_ip, proto_payload, proto_len);
    } else if (hdr->protocol == IP_PROTO_TCP) {
        tcp_receive(hdr->src_ip, proto_payload, proto_len);
    }
}
