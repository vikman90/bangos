#ifndef TARFS_H
#define TARFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "fs/vfs.h"

void tarfs_init(const void *tar_base, size_t tar_size);
int  tarfs_lookup(const char *path, const void **out_data, size_t *out_size);
void tarfs_list_files(void);
vfs_node_t *tarfs_get_vfs_root(void);

#endif /* TARFS_H */
