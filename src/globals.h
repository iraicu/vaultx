#ifndef GLOBALS_H
#define GLOBALS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <omp.h>
#include <time.h>
#include <unistd.h> // For getpid()
#include <string.h> // For strcmp
#include <getopt.h> // For getopt_long
#include <stdbool.h>
#include <fcntl.h>     // For open, O_RDWR, O_CREAT, O_TRUNC
#include <sys/types.h> // For data types
#include <sys/stat.h>  // For file modes
#include <math.h>
#include <errno.h>
#include <sodium.h>
// #include <secp256k1.h>

#include "../blake3/blake3.h" // Include Blake3 header

#ifndef NONCE_SIZE
#define NONCE_SIZE 5 // Default nonce size
#endif

#ifndef RECORD_SIZE
#define RECORD_SIZE 8 // Default record size
#endif

#define HASH_SIZE (RECORD_SIZE - NONCE_SIZE)
#define PREFIX_SIZE 3 // Example prefix size for getBucketIndex

extern int K;

extern unsigned long long num_records_in_bucket;
extern unsigned long long num_records_in_shuffled_bucket;
extern unsigned long long rounds;
extern unsigned long long total_num_buckets;
extern unsigned long long num_buckets_to_read;
extern unsigned long long full_buckets_global;

extern bool DEBUG;
extern bool writeDataTmp;
extern bool writeDataTmpTable2;
extern bool writeDataTable2;
extern bool SEARCH_BATCH;
extern bool SEARCH;
extern bool HASHGEN;
extern bool BENCHMARK;
extern bool CIRCULAR_ARRAY;
extern bool MEMORY_WRITE;
extern bool VERIFY;
extern bool FULL_BUCKETS;

extern size_t BATCH_SIZE;
extern size_t PREFIX_SEARCH_SIZE;

// --- state & counters ---
// extern size_t zero_nonce_count;
// extern size_t count_condition_met;
// extern size_t count_condition_not_met;

extern uint8_t plot_id[32]; // Plot ID for the vault
extern uint8_t hashed_key[32];

// Structure to hold a record with nonce and hash
typedef struct
{
    uint8_t hash[HASH_SIZE];   // 32-byte Blake3 hash
    uint8_t nonce[NONCE_SIZE]; // Nonce to store the seed
} MemoAllRecord;

// Structure to hold a record with nonce
typedef struct
{
    uint8_t nonce[NONCE_SIZE]; // Nonce to store the seed
} MemoRecord;

typedef struct
{
    // uint8_t hash[HASH_SIZE];
    uint8_t nonce1[NONCE_SIZE];
    uint8_t nonce2[NONCE_SIZE];
} MemoTable2Record;

typedef struct
{
    MemoRecord *records;
    size_t count;       // Number of records in the bucket
    size_t count_waste; // Number of records generated but not stored
    bool full;          // Number of records in the bucket
    size_t flush;       // Number of flushes of bucket
} Bucket;

typedef struct
{
    MemoTable2Record *records;
    size_t count; // Number of records in the bucket
    size_t count_waste;
    bool full;
    size_t flush; // Number of flushes of bucket
} BucketTable2;

extern Bucket *buckets;
extern Bucket *buckets_phase2;
extern BucketTable2 *buckets2;
extern BucketTable2 *buckets2_phase2;

#endif