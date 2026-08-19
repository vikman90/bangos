#ifndef NET_NET_H
#define NET_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IP_ADDR(a, b, c, d) \
    ((uint32_t)(((uint32_t)(a) & 0xFF) | \
               (((uint32_t)(b) & 0xFF) << 8) | \
               (((uint32_t)(c) & 0xFF) << 16) | \
               (((uint32_t)(d) & 0xFF) << 24)))

// QEMU SLIRP Default Network Configuration
#define DEFAULT_LOCAL_IP    IP_ADDR(10, 0, 2, 15)
#define DEFAULT_GATEWAY_IP  IP_ADDR(10, 0, 2, 2)
#define DEFAULT_NETMASK     IP_ADDR(255, 255, 255, 0)
#define DEFAULT_DNS_IP      IP_ADDR(10, 0, 2, 3)

typedef struct net_config {
    uint32_t local_ip;
    uint32_t gateway_ip;
    uint32_t netmask;
    uint32_t dns_ip;
    uint8_t  mac[6];
} net_config_t;

// Byte-swapping macros and functions
static inline uint16_t htons(uint16_t val) {
    return (uint16_t)((val << 8) | (val >> 8));
}

static inline uint16_t ntohs(uint16_t val) {
    return htons(val);
}

static inline uint32_t htonl(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

static inline uint32_t ntohl(uint32_t val) {
    return htonl(val);
}

uint16_t      net_checksum(const void *data, size_t len);
void          net_init(void);
void          net_poll(void);
net_config_t *net_get_config(void);
void          ip_to_str(uint32_t ip, char *buf, size_t buf_sz);
uint32_t      str_to_ip(const char *str);

#endif /* NET_NET_H */
