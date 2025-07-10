#ifndef TABLE1_H
#define TABLE1_H

#include "globals.h"
#include "vaultx.h"

// Generate Table1
void generateHashes();
// Insert record into bucket
void insert_record(Bucket* buckets, MemoRecord* record, size_t bucketIndex);

#endif