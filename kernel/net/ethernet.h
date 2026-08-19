#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_ARP   0x0806

#define ETH_ALEN        6
#define ETH_HLEN        14

typedef struct eth_hdr {
    uint8_t  dst_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t ethertype;
} __attribute__((packed)) eth_hdr_t;

int  ethernet_send(const uint8_t *dst_mac, uint16_t ethertype, const void *payload, size_t len);
void ethernet_receive(const uint8_t *packet, size_t len);

#endif /* NET_ETHERNET_H */
