#ifndef GLOBALS_H
#define GLOBALS_H

#include <errno.h>
#include <fcntl.h> // For open, O_RDWR, O_CREAT, O_TRUNC
#include <getopt.h> // For getopt_long
#include <math.h>
#include <omp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strcmp
#include <sys/stat.h> // For file modes
#include <sys/types.h> // For data types
#include <time.h>
#include <unistd.h> // For getpid()

#include "../blake3/blake3.h" // Include Blake3 header

#ifndef NONCE_SIZE
#define NONCE_SIZE 4 // Default nonce size
#endif

#ifndef RECORD_SIZE
#define RECORD_SIZE 8 // Default record size
#endif

#define HASH_SIZE (RECORD_SIZE - NONCE_SIZE)
#define PREFIX_SIZE 3 // Example prefix size for getBucketIndex

extern int K;
extern int MERGE_APPROACH;
extern int current_file;
extern int TOTAL_FILES;
extern int BATCH_MEMORY_MB;
extern int MEMORY_LIMIT_MB;
extern int num_threads;
extern int DIFFICULTY;

extern unsigned long long num_records_in_bucket;
extern unsigned long long num_records_in_shuffled_bucket;
extern unsigned long long rounds;
extern unsigned long long total_buckets;
extern unsigned long long num_buckets_to_read;
extern unsigned long long full_buckets_global;
extern unsigned long long total_nonces;

extern bool DEBUG;
extern bool writeData;
extern bool writeDataFinal;
extern bool writeDataTable2;
extern bool writeDataTable2Tmp;
extern bool SEARCH_BATCH;
extern bool SEARCH;
extern bool HASHGEN;
extern bool BENCHMARK;
extern bool CIRCULAR_ARRAY;
extern bool MEMORY_WRITE;
extern bool VERIFY;
extern bool FULL_BUCKETS;
extern bool ENABLE_DETAILED_METRICS;
extern bool METRICS_OUTPUT_JSON;
extern bool METRICS_OUTPUT_CSV;

extern char* SOURCE;
extern char* DESTINATION;
extern char user[256];

extern size_t BATCH_SIZE;
extern size_t PREFIX_SEARCH_SIZE;

extern uint8_t key[32];

// Structure to hold a record with nonce and hash
typedef struct
{
    uint8_t hash[HASH_SIZE]; // 32-byte Blake3 hash
    uint8_t nonce[NONCE_SIZE]; // Nonce to store the seed
} MemoAllRecord;

// Structure to hold a record with nonce
typedef struct
{
    uint8_t nonce[NONCE_SIZE]; // Nonce to store the seed
} MemoRecord;

typedef struct {
    MemoRecord* record;
    uint8_t hash[HASH_SIZE];
} MemoRecordWithHash;

typedef struct
{
    // uint8_t hash[HASH_SIZE];
    uint8_t nonce1[NONCE_SIZE];
    uint8_t nonce2[NONCE_SIZE];
} MemoTable2Record;

typedef struct
{
    MemoRecord* records;
    size_t count; // Number of records in the bucket
    size_t count_waste; // Number of records generated but not stored
    bool full; // Number of records in the bucket
    size_t flush; // Number of flushes of bucket
} Bucket;

typedef struct
{
    size_t count; // Number of records in the bucket
    size_t count_waste;
    bool full;
    size_t flush; // Number of flushes of bucket
} BucketTable2;

typedef struct {
    uint8_t key[32];
    int K;
} PlotData;

typedef struct
{
    MemoTable2Record* records;
} FileRecords;

typedef struct
{
    MemoTable2Record* buffer;
    MemoTable2Record* mergedBuckets;
    bool readDone;
    bool mergeDone;
    bool writeDone;
    double total_time;
} MergeBatch;

// ============ Performance Metrics Structures ============

typedef struct {
    // Lookup/Search phase metrics
    double total_lookup_time;
    double file_seek_time;
    double io_read_time;
    double buffer_alloc_time;
    double hash_compute_time;
    double memory_search_time;

    uint64_t io_read_calls;
    uint64_t io_seek_calls;
    size_t bytes_read;
    size_t bytes_loaded;

    uint64_t hashes_computed;
    uint64_t lookups_performed;
} LookupMetrics;

typedef struct {
    // Hash generation phase metrics
    double total_hashgen_time;
    double actual_compute_time;
    double thread_wait_time;
    double bucket_insert_time;
    double bucket_lock_time;

    uint64_t total_hashes_generated;
    uint64_t bucket_overflows;
    uint64_t atomic_operations;
    uint64_t full_buckets_count;

    double hash_throughput_MHps;
    double compute_efficiency;
} HashgenMetrics;

typedef struct {
    // Merge phase I/O metrics
    double total_read_time;
    double total_write_time;
    double total_merge_compute_time;

    uint64_t read_syscalls;
    uint64_t write_syscalls;
    uint64_t total_bytes_read;
    uint64_t total_bytes_written;

    double read_throughput_MBps;
    double write_throughput_MBps;

    // Pipeline efficiency
    double pipeline_efficiency;
    uint64_t pipeline_stalls;
} MergeMetrics;

typedef struct {
    // Overall metrics
    LookupMetrics lookup;
    HashgenMetrics hashgen;
    MergeMetrics merge;

    // Thread utilization (per-thread)
    int num_threads;
    double thread_idle_time[256];
    uint64_t thread_wait_events[256];

    // Global timing
    double total_execution_time;
} GlobalMetrics;

extern GlobalMetrics global_metrics;
extern Bucket* buckets;
extern Bucket* buckets_phase2;
extern BucketTable2* buckets2;
extern BucketTable2* buckets_table2;
extern BucketTable2* buckets2_phase2;
extern MemoTable2Record* table2;

#endif
