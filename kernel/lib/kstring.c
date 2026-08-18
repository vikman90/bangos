#include "kstring.h"

int kstrcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int kstrncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

size_t kstrlen(const char *s) {
    if (!s) return 0;
    const char *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    if (!dst || !src || n == 0) return dst;
    size_t i = 0;
    for (; i + 1 < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

char *kstrstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char *)haystack;

    size_t nlen = kstrlen(needle);
    while (*haystack) {
        if (kstrncmp(haystack, needle, nlen) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

char *kstrchr(const char *s, int c) {
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

char *kstrrchr(const char *s, int c) {
    if (!s) return NULL;
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
}

void *kmemset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

void *kmemcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}
