#ifndef TABLE1_H
#define TABLE1_H

#include "globals.h"
#include "vaultx.h"

// Wrapper function to generate hash using crypto's generateBlake3
void generate_hash(uint8_t *nonce, uint8_t *hash);
// Generate Table1
void generateHashes();
// Insert record into bucket
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex);

#endif
