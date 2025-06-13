#ifndef TABLE1_H
#define TABLE1_H

#include "globals.h"
#include "vaultx.h"

void g(uint8_t* nonce, uint8_t* key, uint8_t* hash);

void generateBlake3(uint8_t *record_hash, MemoRecord *record, unsigned long long seed);
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex);
void generateHashes(unsigned long long start, unsigned long long end);
size_t process_memo_records(const char *filename, const size_t BATCH_SIZE);

#endif