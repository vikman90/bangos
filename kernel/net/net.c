#include "net.h"
#include "ethernet.h"
#include "arp.h"
#include "tcp.h"
#include "drivers/virtio_net.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

static net_config_t global_net_config;
static uint8_t poll_rx_buffer[2048];

uint16_t net_checksum(const void *data, size_t len) {
    const uint16_t *buf = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t *)buf;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void net_init(void) {
    kmemset(&global_net_config, 0, sizeof(global_net_config));
    global_net_config.local_ip   = DEFAULT_LOCAL_IP;
    global_net_config.gateway_ip = DEFAULT_GATEWAY_IP;
    global_net_config.netmask    = DEFAULT_NETMASK;
    global_net_config.dns_ip     = DEFAULT_DNS_IP;

    if (virtio_net_is_present()) {
        virtio_net_get_mac(global_net_config.mac);
    } else {
        uint8_t fallback_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
        kmemcpy(global_net_config.mac, fallback_mac, 6);
    }

    char ip_str[32], gw_str[32], dns_str[32];
    ip_to_str(global_net_config.local_ip, ip_str, sizeof(ip_str));
    ip_to_str(global_net_config.gateway_ip, gw_str, sizeof(gw_str));
    ip_to_str(global_net_config.dns_ip, dns_str, sizeof(dns_str));

    kprintf("[Net] TCP/IP Network Subsystem initialized.\n");
    kprintf("[Net] Static Config: IP=%s Gateway=%s DNS=%s MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
            ip_str, gw_str, dns_str,
            global_net_config.mac[0], global_net_config.mac[1], global_net_config.mac[2],
            global_net_config.mac[3], global_net_config.mac[4], global_net_config.mac[5]);

    arp_init();
    tcp_init();
}

void net_poll(void) {
    if (!virtio_net_is_present()) return;

    int bytes = virtio_net_poll_packet(poll_rx_buffer, sizeof(poll_rx_buffer));
    if (bytes > 0) {
        ethernet_receive(poll_rx_buffer, (size_t)bytes);
    }
}

net_config_t *net_get_config(void) {
    return &global_net_config;
}

void ip_to_str(uint32_t ip, char *buf, size_t buf_sz) {
    if (!buf || buf_sz < 16) return;
    uint8_t a = (ip >> 0) & 0xFF;
    uint8_t b = (ip >> 8) & 0xFF;
    uint8_t c = (ip >> 16) & 0xFF;
    uint8_t d = (ip >> 24) & 0xFF;

    // Simple decimal formatting into buffer
    int pos = 0;
    uint8_t octets[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        if (val >= 100) {
            buf[pos++] = '0' + (val / 100);
            val %= 100;
            buf[pos++] = '0' + (val / 10);
            val %= 10;
        } else if (val >= 10) {
            buf[pos++] = '0' + (val / 10);
            val %= 10;
        }
        buf[pos++] = '0' + val;
        if (i < 3) {
            buf[pos++] = '.';
        }
    }
    buf[pos] = '\0';
}

uint32_t str_to_ip(const char *str) {
    if (!str) return 0;
    uint32_t octets[4] = {0, 0, 0, 0};
    int octet_idx = 0;

    while (*str && octet_idx < 4) {
        if (*str >= '0' && *str <= '9') {
            octets[octet_idx] = octets[octet_idx] * 10 + (*str - '0');
        } else if (*str == '.') {
            octet_idx++;
        } else {
            break;
        }
        str++;
    }

    return IP_ADDR(octets[0], octets[1], octets[2], octets[3]);
}
