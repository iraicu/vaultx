#ifndef SHUFFLE_H
#define SHUFFLE_H

#include "globals.h"
#include "vaultx.h"

void shuffle_table2(FILE *fd_src, FILE *fd_dest, size_t buffer_size, size_t records_per_batch, unsigned long long num_buckets_to_read);

#endif // SHUFFLE_H