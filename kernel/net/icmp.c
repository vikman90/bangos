#include "icmp.h"
#include "ipv4.h"
#include "net.h"
#include "drivers/pit.h"
#include "lib/kstring.h"

static volatile bool     ping_reply_received = false;
static volatile uint16_t ping_expected_id = 0;
static volatile uint16_t ping_expected_seq = 0;
static uint16_t          ping_seq_counter = 1;

void icmp_receive(uint32_t src_ip, const uint8_t *payload, size_t len) {
    if (!payload || len < sizeof(icmp_hdr_t)) return;

    const icmp_hdr_t *hdr = (const icmp_hdr_t *)payload;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        // Echo Request -> Construct and send Echo Reply
        uint8_t reply_buf[128];
        if (len > sizeof(reply_buf)) len = sizeof(reply_buf);

        kmemcpy(reply_buf, payload, len);
        icmp_hdr_t *reply_hdr = (icmp_hdr_t *)reply_buf;
        reply_hdr->type = ICMP_TYPE_ECHO_REPLY;
        reply_hdr->code = 0;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = net_checksum(reply_buf, len);

        ipv4_send(src_ip, IP_PROTO_ICMP, reply_buf, len);
    } else if (hdr->type == ICMP_TYPE_ECHO_REPLY) {
        if (ntohs(hdr->id) == ping_expected_id && ntohs(hdr->sequence) == ping_expected_seq) {
            ping_reply_received = true;
        }
    }
}

static inline uint64_t rdtsc_now(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define TSC_PER_MS (500000ULL)

int icmp_ping(uint32_t dst_ip, uint32_t timeout_ms, uint32_t *out_rtt_ms) {
    uint8_t ping_buf[sizeof(icmp_hdr_t) + 32];
    icmp_hdr_t *hdr = (icmp_hdr_t *)ping_buf;

    uint16_t id = 0x4242;
    uint16_t seq = ping_seq_counter++;

    ping_expected_id = id;
    ping_expected_seq = seq;
    ping_reply_received = false;

    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);

    // Fill payload pattern
    for (size_t i = sizeof(icmp_hdr_t); i < sizeof(ping_buf); i++) {
        ping_buf[i] = (uint8_t)('A' + ((i - sizeof(icmp_hdr_t)) % 26));
    }

    hdr->checksum = net_checksum(ping_buf, sizeof(ping_buf));

    if (ipv4_send(dst_ip, IP_PROTO_ICMP, ping_buf, sizeof(ping_buf)) < 0) {
        return -1;
    }

    uint64_t start_tsc = rdtsc_now();
    uint64_t timeout_cycles = (uint64_t)timeout_ms * TSC_PER_MS;
    if (timeout_cycles == 0) timeout_cycles = TSC_PER_MS;

    while ((rdtsc_now() - start_tsc) <= timeout_cycles) {
        net_poll();
        if (ping_reply_received) {
            uint64_t elapsed_cycles = rdtsc_now() - start_tsc;
            if (out_rtt_ms) {
                *out_rtt_ms = (uint32_t)(elapsed_cycles / TSC_PER_MS);
                if (*out_rtt_ms == 0) *out_rtt_ms = 1;
            }
            return 0; // Success
        }
        __asm__ volatile ("pause");
    }

    return -1; // Timeout
}
