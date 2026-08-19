#include "udp.h"
#include "ipv4.h"
#include "net.h"
#include "lib/kstring.h"

#define MAX_UDP_PORTS 16

typedef struct udp_binding {
    uint16_t       port;
    udp_callback_t callback;
    bool           in_use;
} udp_binding_t;

static udp_binding_t udp_bindings[MAX_UDP_PORTS];
static uint8_t       udp_tx_buffer[2048];

int udp_register_port(uint16_t port, udp_callback_t callback) {
    if (!callback) return -1;

    for (int i = 0; i < MAX_UDP_PORTS; i++) {
        if (!udp_bindings[i].in_use) {
            udp_bindings[i].port = port;
            udp_bindings[i].callback = callback;
            udp_bindings[i].in_use = true;
            return 0;
        }
    }
    return -1;
}

void udp_unregister_port(uint16_t port) {
    for (int i = 0; i < MAX_UDP_PORTS; i++) {
        if (udp_bindings[i].in_use && udp_bindings[i].port == port) {
            udp_bindings[i].in_use = false;
            udp_bindings[i].callback = NULL;
            return;
        }
    }
}

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void *payload, size_t len) {
    size_t total_udp_len = sizeof(udp_hdr_t) + len;
    if (total_udp_len > sizeof(udp_tx_buffer)) {
        return -1;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)udp_tx_buffer;
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length = htons((uint16_t)total_udp_len);
    hdr->checksum = 0; // Checksum is optional in IPv4 UDP, set to 0

    if (payload && len > 0) {
        kmemcpy(udp_tx_buffer + sizeof(udp_hdr_t), payload, len);
    }

    return ipv4_send(dst_ip, IP_PROTO_UDP, udp_tx_buffer, total_udp_len);
}

void udp_receive(uint32_t src_ip, const uint8_t *payload, size_t len) {
    if (!payload || len < sizeof(udp_hdr_t)) return;

    const udp_hdr_t *hdr = (const udp_hdr_t *)payload;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t udp_len = ntohs(hdr->length);

    if (udp_len > len || udp_len < sizeof(udp_hdr_t)) return;

    const uint8_t *data = payload + sizeof(udp_hdr_t);
    size_t data_len = udp_len - sizeof(udp_hdr_t);

    for (int i = 0; i < MAX_UDP_PORTS; i++) {
        if (udp_bindings[i].in_use && udp_bindings[i].port == dst_port) {
            if (udp_bindings[i].callback) {
                udp_bindings[i].callback(src_ip, src_port, data, data_len);
            }
            return;
        }
    }
}
