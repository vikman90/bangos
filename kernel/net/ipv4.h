#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

typedef struct ipv4_hdr {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ipv4_hdr_t;

int  ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload, size_t len);
void ipv4_receive(const uint8_t *payload, size_t len);

#endif /* NET_IPV4_H */
