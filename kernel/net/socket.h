#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs/vfs.h"
#include "tcp.h"

#define AF_UNSPEC   0
#define AF_INET     2

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    uint8_t        sin_zero[8];
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

typedef struct net_socket {
    int           domain;
    int           type;
    int           protocol;
    tcp_socket_t *tcp_sock;
    vfs_node_t    vfs_node;
    bool          in_use;
} net_socket_t;

void          socket_subsystem_init(void);
net_socket_t *socket_create(int domain, int type, int protocol);
int           socket_connect(net_socket_t *sock, const struct sockaddr_in *addr);
int64_t       socket_send(net_socket_t *sock, const void *buf, size_t len, int flags);
int64_t       socket_recv(net_socket_t *sock, void *buf, size_t len, int flags);
int           socket_close(net_socket_t *sock);
int           socket_poll(net_socket_t *sock, int events);
net_socket_t *socket_from_vfs_node(vfs_node_t *node);

#endif /* NET_SOCKET_H */
