#ifndef SHUFFLE_H
#define SHUFFLE_H

#include "globals.h"
#include "vaultx.h"

// void shuffle_table1(FILE *fd_src, FILE *fd_dest, size_t buffer_size, size_t records_per_batch, unsigned long long num_buckets_to_read, double start_time, double elapsed_time_io2, double elapsed_time_io2_total);
void shuffle_table2(FILE *fd_src, FILE *fd_dest, size_t buffer_size, size_t records_per_batch, unsigned long long num_buckets_to_read, double start_time, double elapsed_time_io2, double elapsed_time_io2_total);

#endif // SHUFFLE_H