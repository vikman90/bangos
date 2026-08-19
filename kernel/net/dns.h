#ifndef NET_DNS_H
#define NET_DNS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DNS_PORT        53
#define DNS_TYPE_A      1
#define DNS_CLASS_IN    1

typedef struct dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_hdr_t;

int dns_resolve(const char *hostname, uint32_t *out_ip, uint32_t timeout_ms);

#endif /* NET_DNS_H */
