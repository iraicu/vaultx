#ifndef TABLE2_H
#define TABLE2_H

#include "crypto.h"
#include "globals.h"
#include "sort.h"
#include "vaultx.h"

// Wrapper function to generate hash for two nonces
void generate_hash2(uint8_t *nonce1, uint8_t *nonce2, uint8_t *hash);

// In-memory merge approach functions
void findMatches();
int insert_record2_table(MemoTable2Record *table2, BucketTable2 *buckets2,
                         MemoTable2Record *record, size_t bucketIndex);
void writeTable2(uint8_t *plot_id);

// Out-of-memory approach functions
void insert_record2_buckets(BucketTable2 *buckets2, MemoTable2Record *record,
                            size_t bucketIndex);
unsigned long long generate_table2(MemoRecord *sorted_nonces,
                                   size_t num_records_in_bucket);

// Shared utility functions
uint64_t compute_hash_distance(const uint8_t *hash_output,
                               const uint8_t *prev_hash, size_t hash_size);
int print_table2_entry(uint8_t *nonce_output, uint8_t *prev_nonce,
                       const uint8_t *hash_output, const uint8_t *prev_hash,
                       size_t hash_size);
size_t process_memo_records_table2(const char *filename,
                                   const size_t BATCH_SIZE);

#endif
