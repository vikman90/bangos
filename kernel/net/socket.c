#include "socket.h"
#include "net.h"
#include "lib/kstring.h"
#include "drivers/uart.h"

#define MAX_NET_SOCKETS 32

static net_socket_t net_socket_table[MAX_NET_SOCKETS];

static int64_t socket_vfs_read(vfs_node_t *node, uint64_t offset, size_t size, void *buffer) {
    (void)offset;
    net_socket_t *sock = socket_from_vfs_node(node);
    if (!sock) return -9; // -EBADF
    return socket_recv(sock, buffer, size, 0);
}

static int64_t socket_vfs_write(vfs_node_t *node, uint64_t offset, size_t size, const void *buffer) {
    (void)offset;
    net_socket_t *sock = socket_from_vfs_node(node);
    if (!sock) return -9; // -EBADF
    return socket_send(sock, buffer, size, 0);
}

static int socket_vfs_close(vfs_node_t *node) {
    net_socket_t *sock = socket_from_vfs_node(node);
    if (!sock) return -9;
    return socket_close(sock);
}

static vfs_ops_t socket_ops;

void socket_subsystem_init(void) {
    kmemset(net_socket_table, 0, sizeof(net_socket_table));
    socket_ops.open = NULL;
    socket_ops.close = socket_vfs_close;
    socket_ops.read = socket_vfs_read;
    socket_ops.write = socket_vfs_write;
    socket_ops.readdir = NULL;
    socket_ops.finddir = NULL;
    socket_ops.create = NULL;
}

net_socket_t *socket_create(int domain, int type, int protocol) {
    if (domain != AF_INET) return NULL;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return NULL;

    // Dynamically assign function pointers to ensure correct runtime relocation
    socket_ops.open = NULL;
    socket_ops.close = socket_vfs_close;
    socket_ops.read = socket_vfs_read;
    socket_ops.write = socket_vfs_write;
    socket_ops.readdir = NULL;
    socket_ops.finddir = NULL;
    socket_ops.create = NULL;

    for (int i = 0; i < MAX_NET_SOCKETS; i++) {
        if (!net_socket_table[i].in_use) {
            net_socket_t *s = &net_socket_table[i];
            kmemset(s, 0, sizeof(net_socket_t));
            s->in_use = true;
            s->domain = domain;
            s->type = type;
            s->protocol = protocol;

            if (type == SOCK_STREAM) {
                s->tcp_sock = tcp_socket_create();
                if (!s->tcp_sock) {
                    s->in_use = false;
                    return NULL;
                }
            }

            s->vfs_node.type = VFS_CHARDEV;
            s->vfs_node.ops = &socket_ops;
            s->vfs_node.priv_data = s;
            s->vfs_node.refcount = 1;
            kstrncpy(s->vfs_node.name, "socket", sizeof(s->vfs_node.name));

            return s;
        }
    }
    return NULL;
}

int socket_connect(net_socket_t *sock, const struct sockaddr_in *addr) {
    if (!sock || !addr || addr->sin_family != AF_INET) {
        return -22; // -EINVAL
    }

    if (sock->type == SOCK_STREAM) {
        if (!sock->tcp_sock) return -9;
        uint16_t port = ntohs(addr->sin_port);
        uint32_t ip = addr->sin_addr.s_addr;
        if (tcp_connect(sock->tcp_sock, ip, port, 5000) != 0) {
            return -111; // -ECONNREFUSED
        }
        return 0;
    }

    return 0;
}

int64_t socket_send(net_socket_t *sock, const void *buf, size_t len, int flags) {
    (void)flags;
    if (!sock || !buf) return -14; // -EFAULT
    if (len == 0) return 0;

    if (sock->type == SOCK_STREAM) {
        if (!sock->tcp_sock) return -9;
        int res = tcp_send(sock->tcp_sock, buf, len);
        if (res < 0) return -32; // -EPIPE
        return (int64_t)res;
    }

    return (int64_t)len;
}

int64_t socket_recv(net_socket_t *sock, void *buf, size_t len, int flags) {
    if (!sock || !buf) return -14; // -EFAULT
    if (len == 0) return 0;

    if (sock->type == SOCK_STREAM) {
        if (!sock->tcp_sock) return -9;
        int res = tcp_recv(sock->tcp_sock, buf, len, flags);
        return (int64_t)res;
    }

    return 0;
}

int socket_close(net_socket_t *sock) {
    if (!sock || !sock->in_use) return 0;

    if (sock->tcp_sock) {
        tcp_close(sock->tcp_sock);
        sock->tcp_sock = NULL;
    }

    sock->in_use = false;
    return 0;
}

int socket_poll(net_socket_t *sock, int events) {
    (void)events;
    if (!sock || !sock->in_use) return 0;

    int revents = 0;
    if (sock->type == SOCK_STREAM && sock->tcp_sock) {
        if (sock->tcp_sock->rx_len > 0 || sock->tcp_sock->state == TCP_STATE_CLOSE_WAIT) {
            revents |= 0x0001; // POLLIN
        }
        if (sock->tcp_sock->state == TCP_STATE_ESTABLISHED) {
            revents |= 0x0004; // POLLOUT
        }
    }
    return revents;
}

net_socket_t *socket_from_vfs_node(vfs_node_t *node) {
    if (!node || node->ops != &socket_ops) return NULL;
    return (net_socket_t *)node->priv_data;
}
