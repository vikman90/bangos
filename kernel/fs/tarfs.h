#ifndef TARFS_H
#define TARFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void tarfs_init(const void *tar_base, size_t tar_size);
int  tarfs_lookup(const char *path, const void **out_data, size_t *out_size);
void tarfs_list_files(void);

#endif /* TARFS_H */
