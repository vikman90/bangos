#ifndef NET_ARP_H
#define NET_ARP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ARP_HTYPE_ETHERNET  1
#define ARP_PTYPE_IPV4      0x0800
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

typedef struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed)) arp_hdr_t;

void arp_init(void);
void arp_handle_packet(const uint8_t *payload, size_t len);
int  arp_resolve(uint32_t target_ip, uint8_t *out_mac);
void arp_insert_cache(uint32_t ip, const uint8_t *mac);
void arp_request(uint32_t target_ip);

#endif /* NET_ARP_H */
