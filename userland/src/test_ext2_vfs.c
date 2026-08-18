#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s at %s:%d\n", msg, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static void test_open_and_read(void) {
    int fd = open("/mnt/ext2/welcome.txt", O_RDONLY);
    ASSERT(fd >= 3, "open('/mnt/ext2/welcome.txt') returned valid fd >= 3");

    char buf[128];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    ASSERT(bytes > 0, "read() returned positive byte count");
    ASSERT(strstr(buf, "Welcome") != NULL || strstr(buf, "BangOS") != NULL, "read() payload matches expected file content");

    close(fd);
    printf("[PASS] POSIX File Open & Data Read (/mnt/ext2/welcome.txt)\n");
}

static void test_nonexistent_file(void) {
    int fd = open("/mnt/ext2/non_existent_file_xyz.txt", O_RDONLY);
    ASSERT(fd < 0, "open() on non-existent path returns negative value");
    ASSERT(errno == ENOENT, "open() on non-existent path sets errno to ENOENT");
    printf("[PASS] Non-Existent Path Error Propagation (-ENOENT)\n");
}

static void test_lseek(void) {
    int fd = open("/mnt/ext2/sample.txt", O_RDONLY);
    ASSERT(fd >= 3, "open('/mnt/ext2/sample.txt') returned valid fd");

    off_t pos = lseek(fd, 5, SEEK_SET);
    ASSERT(pos == 5, "lseek(SEEK_SET) updated file offset to 5");

    char buf[16];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes = read(fd, buf, 4);
    ASSERT(bytes == 4, "read() after lseek() read exactly 4 bytes");

    pos = lseek(fd, 2, SEEK_CUR);
    ASSERT(pos == 11, "lseek(SEEK_CUR) advanced offset to 11");

    close(fd);
    printf("[PASS] POSIX lseek() SEEK_SET & SEEK_CUR Offset Repositioning\n");
}

static void test_fstat(void) {
    int fd = open("/mnt/ext2/welcome.txt", O_RDONLY);
    ASSERT(fd >= 3, "open() returned valid fd");

    struct stat st;
    memset(&st, 0, sizeof(st));
    int res = fstat(fd, &st);
    ASSERT(res == 0, "fstat() succeeded");
    ASSERT(st.st_size > 0, "fstat() reported positive file size");
    ASSERT(S_ISREG(st.st_mode), "fstat() reported regular file mode");

    close(fd);
    printf("[PASS] POSIX fstat() File Size & Mode Attributes\n");
}

static void test_write_persistence(void) {
    const char *test_path = "/mnt/ext2/posix_test.txt";
    const char *payload = "Ext2 Write Persistence Verified 2026";

    int fd = open(test_path, O_WRONLY | O_CREAT, 0644);
    ASSERT(fd >= 3, "open() with O_CREAT created file");

    ssize_t written = write(fd, payload, strlen(payload));
    ASSERT(written == (ssize_t)strlen(payload), "write() completed all bytes");
    close(fd);

    fd = open(test_path, O_RDONLY);
    ASSERT(fd >= 3, "re-opened created file");

    char read_buf[64];
    memset(read_buf, 0, sizeof(read_buf));
    ssize_t r = read(fd, read_buf, sizeof(read_buf) - 1);
    ASSERT(r == written, "read() byte count matched written count");
    ASSERT(strcmp(read_buf, payload) == 0, "readback payload matches written bytes");
    close(fd);

    printf("[PASS] POSIX File Creation, Persistence & Readback (/mnt/ext2/posix_test.txt)\n");
}

static void test_close_ebadf(void) {
    int fd = open("/mnt/ext2/sample.txt", O_RDONLY);
    ASSERT(fd >= 3, "open() returned valid fd");
    close(fd);

    char buf[16];
    ssize_t r = read(fd, buf, sizeof(buf));
    ASSERT(r < 0, "read() on closed fd failed");
    ASSERT(errno == EBADF, "read() on closed fd set errno to EBADF");
    printf("[PASS] Closed File Descriptor Error Handling (-EBADF)\n");
}

int main(void) {
    printf("\n======================================================\n");
    printf("     BangOS ext2 Filesystem & VFS POSIX Spec Suite    \n");
    printf("======================================================\n\n");

    test_open_and_read();
    test_nonexistent_file();
    test_lseek();
    test_fstat();
    test_write_persistence();
    test_close_ebadf();

    printf("\n[SUCCESS] All ext2 and VFS specification tests evaluated to PASS!\n\n");
    return 0;
}
