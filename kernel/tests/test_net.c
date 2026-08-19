#include "ktest.h"
#include "drivers/pci.h"
#include "drivers/virtio_net.h"
#include "net/net.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "lib/kstring.h"

static bool test_pci_scan_and_virtio_probe(void) {
    KTEST_ASSERT(pci_get_device_count() > 0);
    pci_device_t *net_dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_DEV_NET_LEGACY);
    if (!net_dev) {
        net_dev = pci_find_class(0x02, 0xFF);
    }
    KTEST_ASSERT_NOT_NULL(net_dev);
    KTEST_ASSERT(virtio_net_is_present());

    uint8_t mac[6];
    kmemset(mac, 0, sizeof(mac));
    virtio_net_get_mac(mac);
    // MAC address should not be all zeros
    uint32_t mac_sum = mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
    KTEST_ASSERT(mac_sum > 0);

    return true;
}

static bool test_net_checksum_and_ip_utils(void) {
    // Test RFC 1071 16-bit one's complement checksum
    uint8_t test_vec[] = { 0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00, 0x40, 0x06, 0x00, 0x00, 0xac, 0x10, 0x0a, 0x63, 0xac, 0x10, 0x0a, 0x0c };
    uint16_t csum = net_checksum(test_vec, sizeof(test_vec));
    KTEST_ASSERT_NE(csum, 0);

    // IP String conversion
    uint32_t ip = IP_ADDR(192, 168, 1, 100);
    char ip_str[32];
    ip_to_str(ip, ip_str, sizeof(ip_str));
    KTEST_ASSERT_EQ(kstrcmp(ip_str, "192.168.1.100"), 0);

    uint32_t parsed_ip = str_to_ip("192.168.1.100");
    KTEST_ASSERT_EQ(parsed_ip, ip);

    return true;
}

static bool test_arp_cache_operations(void) {
    uint32_t test_ip = IP_ADDR(10, 0, 2, 88);
    uint8_t test_mac[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

    arp_insert_cache(test_ip, test_mac);

    uint8_t resolved_mac[6];
    kmemset(resolved_mac, 0, sizeof(resolved_mac));
    int res = arp_resolve(test_ip, resolved_mac);
    KTEST_ASSERT_EQ(res, 0);
    KTEST_ASSERT_EQ(resolved_mac[0], 0x00);
    KTEST_ASSERT_EQ(resolved_mac[1], 0x11);
    KTEST_ASSERT_EQ(resolved_mac[5], 0x55);

    return true;
}

static bool test_tcp_state_machine(void) {
    tcp_socket_t *sock = tcp_socket_create();
    KTEST_ASSERT_NOT_NULL(sock);
    KTEST_ASSERT_EQ(sock->state, TCP_STATE_CLOSED);
    KTEST_ASSERT(sock->local_port >= 49152);

    // Test socket layer allocation
    net_socket_t *nsock = socket_create(AF_INET, SOCK_STREAM, 0);
    KTEST_ASSERT_NOT_NULL(nsock);
    KTEST_ASSERT_NOT_NULL(nsock->tcp_sock);
    KTEST_ASSERT_EQ(nsock->domain, AF_INET);
    KTEST_ASSERT_EQ(nsock->type, SOCK_STREAM);

    socket_close(nsock);
    return true;
}

bool test_net_all(void) {
    KTEST_RUN(test_pci_scan_and_virtio_probe);
    KTEST_RUN(test_net_checksum_and_ip_utils);
    KTEST_RUN(test_arp_cache_operations);
    KTEST_RUN(test_tcp_state_machine);
    return true;
}
