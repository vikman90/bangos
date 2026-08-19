#include "tcp.h"
#include "ipv4.h"
#include "net.h"
#include "drivers/pit.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

static tcp_socket_t tcp_sockets[TCP_MAX_SOCKETS];
static uint16_t     ephemeral_port_counter = 49152;
static uint8_t      tcp_tx_buffer[2048];

void tcp_init(void) {
    kmemset(tcp_sockets, 0, sizeof(tcp_sockets));
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_sockets[i].id = i;
        tcp_sockets[i].in_use = false;
        tcp_sockets[i].state = TCP_STATE_CLOSED;
    }
}

tcp_socket_t *tcp_socket_create(void) {
    net_config_t *cfg = net_get_config();

    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].in_use) {
            tcp_socket_t *s = &tcp_sockets[i];
            kmemset(s, 0, sizeof(tcp_socket_t));
            s->id = i;
            s->in_use = true;
            s->state = TCP_STATE_CLOSED;
            s->local_ip = cfg->local_ip;
            s->local_port = ephemeral_port_counter++;
            if (ephemeral_port_counter > 65000) {
                ephemeral_port_counter = 49152;
            }
            s->remote_window = TCP_RX_BUF_SIZE;
            return s;
        }
    }
    return NULL;
}

tcp_socket_t *tcp_find_socket(uint16_t local_port, uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].in_use && tcp_sockets[i].local_port == local_port) {
            if (tcp_sockets[i].remote_ip == 0 ||
                (tcp_sockets[i].remote_ip == remote_ip && tcp_sockets[i].remote_port == remote_port)) {
                return &tcp_sockets[i];
            }
        }
    }
    return NULL;
}

static uint16_t tcp_compute_checksum(uint32_t src_ip, uint32_t dst_ip, const void *tcp_seg, size_t tcp_len) {
    uint8_t pseudo_buf[2048 + sizeof(tcp_pseudo_hdr_t)];
    if (tcp_len + sizeof(tcp_pseudo_hdr_t) > sizeof(pseudo_buf)) return 0;

    tcp_pseudo_hdr_t *phdr = (tcp_pseudo_hdr_t *)pseudo_buf;
    phdr->src_ip = src_ip;
    phdr->dst_ip = dst_ip;
    phdr->zero = 0;
    phdr->protocol = IP_PROTO_TCP;
    phdr->tcp_length = htons((uint16_t)tcp_len);

    kmemcpy(pseudo_buf + sizeof(tcp_pseudo_hdr_t), tcp_seg, tcp_len);

    return net_checksum(pseudo_buf, sizeof(tcp_pseudo_hdr_t) + tcp_len);
}

static int tcp_send_segment(tcp_socket_t *sock, uint8_t flags, const void *payload, size_t len) {
    if (!sock || (sizeof(tcp_hdr_t) + len) > sizeof(tcp_tx_buffer)) return -1;

    tcp_hdr_t *hdr = (tcp_hdr_t *)tcp_tx_buffer;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq_num = htonl(sock->seq_num);
    hdr->ack_num = htonl(sock->ack_num);
    hdr->data_offset_reserved = (5 << 4); // 20 bytes
    hdr->flags = flags;
    hdr->window_size = htons((uint16_t)(TCP_RX_BUF_SIZE - sock->rx_len));
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;

    if (payload && len > 0) {
        kmemcpy(tcp_tx_buffer + sizeof(tcp_hdr_t), payload, len);
    }

    size_t total_len = sizeof(tcp_hdr_t) + len;
    hdr->checksum = tcp_compute_checksum(sock->local_ip, sock->remote_ip, tcp_tx_buffer, total_len);

    int res = ipv4_send(sock->remote_ip, IP_PROTO_TCP, tcp_tx_buffer, total_len);
    if (res >= 0) {
        if (flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) {
            sock->seq_num++;
        }
        sock->seq_num += (uint32_t)len;
    }

    return res;
}

static inline uint64_t rdtsc_now(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define TSC_PER_MS (500000ULL)

int tcp_connect(tcp_socket_t *sock, uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    if (!sock) return -1;

    sock->remote_ip = dst_ip;
    sock->remote_port = dst_port;
    sock->seq_num = 1000 + (uint32_t)(rdtsc_now() & 0xFFFF);
    sock->ack_num = 0;
    sock->state = TCP_STATE_SYN_SENT;

    if (tcp_send_segment(sock, TCP_FLAG_SYN, NULL, 0) < 0) {
        sock->state = TCP_STATE_CLOSED;
        return -1;
    }

    uint64_t start_tsc = rdtsc_now();
    uint64_t timeout_cycles = (uint64_t)timeout_ms * TSC_PER_MS;
    if (timeout_cycles == 0) timeout_cycles = TSC_PER_MS;

    while ((rdtsc_now() - start_tsc) <= timeout_cycles) {
        net_poll();
        if (sock->state == TCP_STATE_ESTABLISHED) {
            return 0; // Connected!
        }
        if (sock->state == TCP_STATE_CLOSED) {
            return -1; // Connection refused / Reset
        }
        __asm__ volatile ("pause");
    }

    sock->state = TCP_STATE_CLOSED;
    return -1; // Timeout
}

int tcp_send(tcp_socket_t *sock, const void *data, size_t len) {
    if (!sock || !data || len == 0) return 0;
    if (sock->state != TCP_STATE_ESTABLISHED && sock->state != TCP_STATE_CLOSE_WAIT) {
        return -1;
    }

    const uint8_t *ptr = (const uint8_t *)data;
    size_t remaining = len;

    while (remaining > 0) {
        size_t chunk = (remaining > TCP_DEFAULT_MSS) ? TCP_DEFAULT_MSS : remaining;
        if (tcp_send_segment(sock, TCP_FLAG_ACK | TCP_FLAG_PSH, ptr, chunk) < 0) {
            return -1;
        }
        ptr += chunk;
        remaining -= chunk;
        net_poll();
    }

    return (int)len;
}

int tcp_recv(tcp_socket_t *sock, void *buf, size_t len, int flags) {
    (void)flags;
    if (!sock || !buf || len == 0) return 0;

    // If buffer empty and socket still connected, poll briefly for incoming data
    if (sock->rx_len == 0 && sock->state == TCP_STATE_ESTABLISHED) {
        uint64_t start_tsc = rdtsc_now();
        uint64_t poll_cycles = 2000ULL * TSC_PER_MS; // 2.0s poll window
        while ((rdtsc_now() - start_tsc) < poll_cycles) {
            net_poll();
            if (sock->rx_len > 0 || sock->state != TCP_STATE_ESTABLISHED) {
                break;
            }
            __asm__ volatile ("pause");
        }
    }

    if (sock->rx_len == 0) {
        if (sock->state == TCP_STATE_CLOSE_WAIT || sock->state == TCP_STATE_CLOSED) {
            return 0; // EOF
        }
        return 0;
    }

    size_t to_copy = (len < sock->rx_len) ? len : sock->rx_len;
    uint8_t *dst = (uint8_t *)buf;

    for (size_t i = 0; i < to_copy; i++) {
        dst[i] = sock->rx_buf[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % TCP_RX_BUF_SIZE;
    }

    sock->rx_len -= (uint32_t)to_copy;
    return (int)to_copy;
}

int tcp_close(tcp_socket_t *sock) {
    if (!sock || !sock->in_use) return 0;

    if (sock->state == TCP_STATE_ESTABLISHED) {
        sock->state = TCP_STATE_FIN_WAIT_1;
        tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);

        uint64_t start_tsc = rdtsc_now();
        uint64_t grace_cycles = 500ULL * TSC_PER_MS; // 500ms grace period
        while ((rdtsc_now() - start_tsc) < grace_cycles) {
            net_poll();
            if (sock->state == TCP_STATE_CLOSED || sock->state == TCP_STATE_TIME_WAIT) {
                break;
            }
            __asm__ volatile ("pause");
        }
    } else if (sock->state == TCP_STATE_CLOSE_WAIT) {
        sock->state = TCP_STATE_LAST_ACK;
        tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);

        uint64_t start_tsc = rdtsc_now();
        uint64_t grace_cycles = 500ULL * TSC_PER_MS;
        while ((rdtsc_now() - start_tsc) < grace_cycles) {
            net_poll();
            if (sock->state == TCP_STATE_CLOSED) break;
            __asm__ volatile ("pause");
        }
    }

    sock->state = TCP_STATE_CLOSED;
    sock->in_use = false;
    return 0;
}

void tcp_receive(uint32_t src_ip, const uint8_t *payload, size_t len) {
    if (!payload || len < sizeof(tcp_hdr_t)) return;

    const tcp_hdr_t *hdr = (const tcp_hdr_t *)payload;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint32_t seq = ntohl(hdr->seq_num);
    uint32_t ack = ntohl(hdr->ack_num);
    uint8_t  flags = hdr->flags;

    size_t header_len = (hdr->data_offset_reserved >> 4) * 4;
    if (header_len < sizeof(tcp_hdr_t) || header_len > len) return;

    tcp_socket_t *sock = tcp_find_socket(dst_port, src_ip, src_port);
    if (!sock) return;

    if (flags & TCP_FLAG_RST) {
        sock->state = TCP_STATE_CLOSED;
        return;
    }

    switch (sock->state) {
        case TCP_STATE_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                sock->ack_num = seq + 1;
                sock->state = TCP_STATE_ESTABLISHED;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_STATE_ESTABLISHED: {
            size_t data_len = len - header_len;
            const uint8_t *data = payload + header_len;

            if (data_len > 0) {
                // Copy to socket RX circular buffer
                for (size_t i = 0; i < data_len; i++) {
                    if (sock->rx_len < TCP_RX_BUF_SIZE) {
                        sock->rx_buf[sock->rx_tail] = data[i];
                        sock->rx_tail = (sock->rx_tail + 1) % TCP_RX_BUF_SIZE;
                        sock->rx_len++;
                    }
                }
                sock->ack_num = seq + (uint32_t)data_len;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
            }

            if (flags & TCP_FLAG_FIN) {
                sock->ack_num = seq + (data_len > 0 ? (uint32_t)data_len : 1);
                sock->state = TCP_STATE_CLOSE_WAIT;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
            }
            break;
        }

        case TCP_STATE_FIN_WAIT_1:
            if (flags & TCP_FLAG_ACK) {
                if (flags & TCP_FLAG_FIN) {
                    sock->ack_num = seq + 1;
                    sock->state = TCP_STATE_TIME_WAIT;
                    tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
                } else {
                    sock->state = TCP_STATE_FIN_WAIT_2;
                }
            }
            break;

        case TCP_STATE_FIN_WAIT_2:
            if (flags & TCP_FLAG_FIN) {
                sock->ack_num = seq + 1;
                sock->state = TCP_STATE_TIME_WAIT;
                tcp_send_segment(sock, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_STATE_LAST_ACK:
            if (flags & TCP_FLAG_ACK) {
                sock->state = TCP_STATE_CLOSED;
            }
            break;

        default:
            (void)ack;
            break;
    }
}
