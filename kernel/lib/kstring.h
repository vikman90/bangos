#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int    kstrcmp(const char *s1, const char *s2);
int    kstrncmp(const char *s1, const char *s2, size_t n);
size_t kstrlen(const char *s);
char  *kstrncpy(char *dst, const char *src, size_t n);
void  *kmemset(void *s, int c, size_t n);
void  *kmemcpy(void *dest, const void *src, size_t n);

#endif /* KSTRING_H */
