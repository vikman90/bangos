#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ANSI_GREEN "\033[32m"
#define ANSI_RED   "\033[31m"
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD  "\033[1m"

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, name) do { \
    if (cond) { \
        printf(ANSI_GREEN "[PASS] " ANSI_RESET "%s\n", name); \
        fflush(stdout); \
        g_passed++; \
    } else { \
        printf(ANSI_RED "[FAIL] " ANSI_RESET "%s (line %d)\n", name, __LINE__); \
        fflush(stdout); \
        g_failed++; \
    } \
} while (0)

int main(void) {
    printf("\n" ANSI_BOLD "======================================================\n");
    printf("     BangOS POSIX Socket Specification Test Suite     \n");
    printf("======================================================\n" ANSI_RESET "\n");
    fflush(stdout);

    // 1. Socket creation test
    int sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(sock_tcp >= 3, "POSIX TCP Stream Socket Creation (AF_INET, SOCK_STREAM)");

    int sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(sock_udp >= 3, "POSIX UDP Datagram Socket Creation (AF_INET, SOCK_DGRAM)");

    // 2. Bad domain / invalid type rejection
    int sock_bad = socket(99, SOCK_STREAM, 0);
    TEST_ASSERT(sock_bad < 0, "Unsupported Address Family Rejection (-EAFNOSUPPORT)");

    // 3. Bad FD validation
    ssize_t send_bad = send(-1, "test", 4, 0);
    TEST_ASSERT(send_bad < 0, "Bad File Descriptor Error Propagation (-EBADF)");

    // 4. Null pointer safety
    int conn_null = connect(sock_tcp, NULL, sizeof(struct sockaddr_in));
    TEST_ASSERT(conn_null < 0, "Null Pointer Bounds Safety Checking (-EFAULT)");

    // 5. Zero-length send/recv
    ssize_t zero_send = send(sock_tcp, "test", 0, 0);
    TEST_ASSERT(zero_send == 0, "Zero-Length Network Socket I/O Handling");

    // 6. Socket close & release
    int close_tcp = close(sock_tcp);
    int close_udp = close(sock_udp);
    TEST_ASSERT(close_tcp == 0 && close_udp == 0, "POSIX Socket Teardown & Descriptor Release");

    printf("\n------------------------------------------------------\n");
    printf("[SUMMARY] Passed: %d | Failed: %d\n", g_passed, g_failed);
    printf("======================================================\n");

    if (g_failed == 0) {
        printf(ANSI_BOLD ANSI_GREEN "[SUCCESS] All socket specification tests evaluated to PASS!\n\n" ANSI_RESET);
        fflush(stdout);
        return 0;
    } else {
        printf(ANSI_BOLD ANSI_RED "[FAILURE] Detected %d failed assertions in socket tests!\n\n" ANSI_RESET, g_failed);
        fflush(stdout);
        return 1;
    }
}
