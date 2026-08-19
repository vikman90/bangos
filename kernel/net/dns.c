#include "dns.h"
#include "udp.h"
#include "net.h"
#include "drivers/pit.h"
#include "drivers/uart.h"
#include "lib/kstring.h"

#define DNS_CLIENT_PORT 53535

static volatile bool     dns_resolved = false;
static volatile uint32_t dns_result_ip = 0;
static uint16_t          dns_query_id = 0x5432;

static void dns_udp_callback(uint32_t src_ip, uint16_t src_port, const uint8_t *data, size_t len) {
    (void)src_ip; (void)src_port;
    if (!data || len < sizeof(dns_hdr_t)) return;

    const dns_hdr_t *hdr = (const dns_hdr_t *)data;
    if (ntohs(hdr->id) != dns_query_id) return;

    uint16_t ancount = ntohs(hdr->ancount);
    uint16_t qdcount = ntohs(hdr->qdcount);
    if (ancount == 0) return;

    // Skip Header
    const uint8_t *ptr = data + sizeof(dns_hdr_t);
    const uint8_t *end = data + len;

    // Skip Question Section
    for (uint16_t q = 0; q < qdcount; q++) {
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2; // Pointer
                break;
            }
            ptr += (*ptr + 1);
        }
        if (ptr < end && *ptr == 0) {
            ptr++; // Skip trailing zero byte
        }
        ptr += 4; // Skip QTYPE (2) and QCLASS (2)
    }

    // Parse Answer Section
    for (uint16_t a = 0; a < ancount; a++) {
        if (ptr >= end) break;

        // Skip Name
        if ((*ptr & 0xC0) == 0xC0) {
            ptr += 2;
        } else {
            while (ptr < end && *ptr != 0) {
                ptr += (*ptr + 1);
            }
            if (ptr < end && *ptr == 0) ptr++;
        }

        if (ptr + 10 > end) break;

        uint16_t type = (uint16_t)((ptr[0] << 8) | ptr[1]);
        // uint16_t class_in = (uint16_t)((ptr[2] << 8) | ptr[3]);
        // uint32_t ttl = (uint32_t)((ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7]);
        uint16_t rdlength = (uint16_t)((ptr[8] << 8) | ptr[9]);
        ptr += 10;

        if (type == DNS_TYPE_A && rdlength == 4 && (ptr + 4 <= end)) {
            dns_result_ip = IP_ADDR(ptr[0], ptr[1], ptr[2], ptr[3]);
            dns_resolved = true;
            return;
        }

        ptr += rdlength;
    }
}

static size_t encode_qname(const char *hostname, uint8_t *out_buf, size_t buf_sz) {
    size_t out_idx = 0;
    const char *start = hostname;

    while (*hostname) {
        if (*hostname == '.') {
            size_t label_len = hostname - start;
            if (label_len > 63 || (out_idx + label_len + 1) >= buf_sz) return 0;
            out_buf[out_idx++] = (uint8_t)label_len;
            for (size_t i = 0; i < label_len; i++) {
                out_buf[out_idx++] = (uint8_t)start[i];
            }
            start = hostname + 1;
        }
        hostname++;
    }

    size_t label_len = hostname - start;
    if (label_len > 0) {
        if (label_len > 63 || (out_idx + label_len + 2) >= buf_sz) return 0;
        out_buf[out_idx++] = (uint8_t)label_len;
        for (size_t i = 0; i < label_len; i++) {
            out_buf[out_idx++] = (uint8_t)start[i];
        }
    }

    out_buf[out_idx++] = 0; // Terminating zero label
    return out_idx;
}

static inline uint64_t rdtsc_now(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define TSC_PER_MS (500000ULL)

int dns_resolve(const char *hostname, uint32_t *out_ip, uint32_t timeout_ms) {
    if (!hostname || !out_ip) return -1;

    // Check if input is already an IPv4 numeric literal
    bool is_numeric = true;
    for (const char *p = hostname; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || *p == '.')) {
            is_numeric = false;
            break;
        }
    }
    if (is_numeric) {
        *out_ip = str_to_ip(hostname);
        return 0;
    }

    dns_resolved = false;
    dns_result_ip = 0;
    dns_query_id++;

    udp_register_port(DNS_CLIENT_PORT, dns_udp_callback);

    uint8_t query_buf[512];
    kmemset(query_buf, 0, sizeof(query_buf));

    dns_hdr_t *hdr = (dns_hdr_t *)query_buf;
    hdr->id = htons(dns_query_id);
    hdr->flags = htons(0x0100); // Standard recursive query
    hdr->qdcount = htons(1);

    size_t qname_len = encode_qname(hostname, query_buf + sizeof(dns_hdr_t), sizeof(query_buf) - sizeof(dns_hdr_t));
    if (qname_len == 0) {
        udp_unregister_port(DNS_CLIENT_PORT);
        return -1;
    }

    size_t offset = sizeof(dns_hdr_t) + qname_len;
    // QTYPE = A (1)
    query_buf[offset++] = 0x00;
    query_buf[offset++] = 0x01;
    // QCLASS = IN (1)
    query_buf[offset++] = 0x00;
    query_buf[offset++] = 0x01;

    net_config_t *cfg = net_get_config();
    if (udp_send(cfg->dns_ip, DNS_CLIENT_PORT, DNS_PORT, query_buf, offset) < 0) {
        udp_unregister_port(DNS_CLIENT_PORT);
        return -1;
    }

    uint64_t start_tsc = rdtsc_now();
    uint64_t timeout_cycles = (uint64_t)timeout_ms * TSC_PER_MS;
    if (timeout_cycles == 0) timeout_cycles = TSC_PER_MS;

    while ((rdtsc_now() - start_tsc) <= timeout_cycles) {
        net_poll();
        if (dns_resolved) {
            *out_ip = dns_result_ip;
            udp_unregister_port(DNS_CLIENT_PORT);
            return 0;
        }
        __asm__ volatile ("pause");
    }

    udp_unregister_port(DNS_CLIENT_PORT);
    return -1; // Timeout
}
