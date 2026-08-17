#include "ktest.h"
#include "lib/kstring.h"

static bool test_kstrlen_basic(void) {
    KTEST_ASSERT_EQ(kstrlen(""), 0);
    KTEST_ASSERT_EQ(kstrlen("a"), 1);
    KTEST_ASSERT_EQ(kstrlen("BangOS"), 6);
    KTEST_ASSERT_EQ(kstrlen("1234567890"), 10);
    return true;
}

static bool test_kstrcmp_basic(void) {
    KTEST_ASSERT_EQ(kstrcmp("", ""), 0);
    KTEST_ASSERT_EQ(kstrcmp("abc", "abc"), 0);
    KTEST_ASSERT(kstrcmp("abc", "abd") < 0);
    KTEST_ASSERT(kstrcmp("abd", "abc") > 0);
    KTEST_ASSERT(kstrcmp("abc", "abcd") < 0);
    KTEST_ASSERT(kstrcmp("abcd", "abc") > 0);
    return true;
}

static bool test_kstrncmp_basic(void) {
    KTEST_ASSERT_EQ(kstrncmp("abcdef", "abcxyz", 3), 0);
    KTEST_ASSERT(kstrncmp("abcdef", "abcxyz", 4) != 0);
    KTEST_ASSERT_EQ(kstrncmp("same", "same", 10), 0);
    KTEST_ASSERT_EQ(kstrncmp("different", "diff", 0), 0);
    return true;
}

static bool test_kstrncpy_basic(void) {
    char buf[16];
    kmemset(buf, 0xFF, sizeof(buf));
    char *res = kstrncpy(buf, "hello", sizeof(buf));
    KTEST_ASSERT_EQ(res, buf);
    KTEST_ASSERT_EQ(kstrcmp(buf, "hello"), 0);
    KTEST_ASSERT_EQ(buf[5], '\0');

    char small[4];
    kmemset(small, 0xAA, sizeof(small));
    kstrncpy(small, "toolongstring", sizeof(small));
    KTEST_ASSERT_EQ(kstrcmp(small, "too"), 0);
    KTEST_ASSERT_EQ(small[3], '\0');
    return true;
}

static bool test_kmemset_kmemcpy_basic(void) {
    uint8_t src[32];
    uint8_t dst[32];

    for (int i = 0; i < 32; i++) {
        src[i] = (uint8_t)(i * 7 + 3);
    }
    kmemset(dst, 0, sizeof(dst));
    for (int i = 0; i < 32; i++) {
        KTEST_ASSERT_EQ(dst[i], 0);
    }

    kmemcpy(dst, src, sizeof(src));
    for (int i = 0; i < 32; i++) {
        KTEST_ASSERT_EQ(dst[i], src[i]);
    }
    return true;
}

bool test_kstring_all(void) {
    kprintf("\n--- [KTEST] Running Kernel String & Memory Library Tests ---\n");
    KTEST_RUN(test_kstrlen_basic);
    KTEST_RUN(test_kstrcmp_basic);
    KTEST_RUN(test_kstrncmp_basic);
    KTEST_RUN(test_kstrncpy_basic);
    KTEST_RUN(test_kmemset_kmemcpy_basic);
    return true;
}
