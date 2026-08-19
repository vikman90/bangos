#ifndef NET_UDP_H
#define NET_UDP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;

typedef struct udp_pseudo_hdr {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t udp_length;
} __attribute__((packed)) udp_pseudo_hdr_t;

typedef void (*udp_callback_t)(uint32_t src_ip, uint16_t src_port, const uint8_t *data, size_t len);

int  udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void *payload, size_t len);
void udp_receive(uint32_t src_ip, const uint8_t *payload, size_t len);
int  udp_register_port(uint16_t port, udp_callback_t callback);
void udp_unregister_port(uint16_t port);

#endif /* NET_UDP_H */
