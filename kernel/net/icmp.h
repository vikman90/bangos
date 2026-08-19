#ifndef NET_ICMP_H
#define NET_ICMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ICMP_TYPE_ECHO_REPLY    0
#define ICMP_TYPE_ECHO_REQUEST  8

typedef struct icmp_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_hdr_t;

void icmp_receive(uint32_t src_ip, const uint8_t *payload, size_t len);
int  icmp_ping(uint32_t dst_ip, uint32_t timeout_ms, uint32_t *out_rtt_ms);

#endif /* NET_ICMP_H */
