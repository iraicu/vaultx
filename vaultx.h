#ifndef VAULTX_H
#define VAULTX_H

#include <stdio.h>
#include <stdlib.h>
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

#ifdef __linux__
#include <linux/fs.h> // Provides `syncfs` on Linux
#endif

#ifdef __cplusplus
// Your C++-specific code here
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif

#include "blake3/blake3.h" // Include Blake3 header

#ifndef NONCE_SIZE
#define NONCE_SIZE 5 // Default nonce size
#endif

#ifndef RECORD_SIZE
#define RECORD_SIZE 8 // Default record size
#endif

#define HASH_SIZE (RECORD_SIZE - NONCE_SIZE)
#define PREFIX_SIZE 3 // Example prefix size for getBucketIndex

unsigned long long num_buckets = 1;
unsigned long long num_records_in_bucket = 1;
unsigned long long rounds = 1;

size_t BATCH_SIZE = 1024;

bool VERIFY = false;
bool DEBUG = false;
bool writeData = false;
bool writeDataFinal = false;
bool MEMORY_WRITE = true;
bool CIRCULAR_ARRAY = false;
bool BENCHMARK = false;
bool HASHGEN = true;
bool SEARCH = false;
bool SEARCH_BATCH = false;
bool TABLE2 = true; 
size_t PREFIX_SEARCH_SIZE = 1;
int NUM_THREADS = 0;

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
    MemoRecord *records;
    size_t count; // Number of records in the bucket
    size_t flush; // Number of flushes of bucket
} Bucket;

int hamming_distance(int x, int y);

#endif // VAULTX_H