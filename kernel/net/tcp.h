#ifndef NET_TCP_H
#define NET_TCP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TCP_FLAG_FIN    0x01
#define TCP_FLAG_SYN    0x02
#define TCP_FLAG_RST    0x04
#define TCP_FLAG_PSH    0x08
#define TCP_FLAG_ACK    0x10
#define TCP_FLAG_URG    0x20

typedef enum tcp_state {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

typedef struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_hdr_t;

typedef struct tcp_pseudo_hdr {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_length;
} __attribute__((packed)) tcp_pseudo_hdr_t;

#define TCP_RX_BUF_SIZE     16384
#define TCP_MAX_SOCKETS     16
#define TCP_DEFAULT_MSS     1460

typedef struct tcp_socket {
    int          id;
    bool         in_use;
    tcp_state_t  state;
    
    uint32_t     local_ip;
    uint16_t     local_port;
    uint32_t     remote_ip;
    uint16_t     remote_port;
    
    uint32_t     seq_num;
    uint32_t     ack_num;
    uint16_t     remote_window;
    
    uint8_t      rx_buf[TCP_RX_BUF_SIZE];
    uint32_t     rx_head;
    uint32_t     rx_tail;
    uint32_t     rx_len;
    
    uint64_t     last_activity_tick;
} tcp_socket_t;

void          tcp_init(void);
tcp_socket_t *tcp_socket_create(void);
int           tcp_connect(tcp_socket_t *sock, uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);
int           tcp_send(tcp_socket_t *sock, const void *data, size_t len);
int           tcp_recv(tcp_socket_t *sock, void *buf, size_t len, int flags);
int           tcp_close(tcp_socket_t *sock);
void          tcp_receive(uint32_t src_ip, const uint8_t *payload, size_t len);
tcp_socket_t *tcp_find_socket(uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);

#endif /* NET_TCP_H */
