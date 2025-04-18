#ifndef TABLE2_H
#define TABLE2_H

#include "globals.h"
#include "vaultx.h"

int compare_memo_all_record(const void *a, const void *b);
MemoRecord *sort_bucket_records(const MemoRecord *unsorted, size_t total_records);
uint64_t compute_hash_distance(const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size);
int print_table2_entry(const uint8_t *nonce_output, const uint8_t *prev_nonce, const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size);
void insert_record_table2(BucketTable2 *buckets2, uint8_t nonce1[NONCE_SIZE], uint8_t nonce2[NONCE_SIZE], size_t bucket_index);
size_t generate_table2(const char *filename, const char *filename_table2, int num_threads, int num_threads_io);

#endif