#include "globals.h"
#include <stdbool.h>

// Defaults
int K = 24;
int current_file = 1;
int MERGE_APPROACH = 0;
int TOTAL_FILES = 2;
int BATCH_MEMORY_MB = 256;
int MEMORY_LIMIT_MB = 307200;
int num_threads = 8;
int DIFFICULTY = 0;

unsigned long long total_nonces;
unsigned long long num_records_in_bucket = 1;
unsigned long long num_records_in_shuffled_bucket = 1;
unsigned long long total_buckets = 1;
unsigned long long rounds = 1; // filesize : memorysize
unsigned long long num_buckets_to_read = 1;
unsigned long long full_buckets_global = 0;
unsigned long long MEMORY_SIZE_bytes = 0;

bool DEBUG = false;
bool MONITOR = false;
bool BENCHMARK = false;
bool CIRCULAR_ARRAY = false;
bool HASHGEN = true;
bool MEMORY_WRITE = true;
bool writeDataTmp = false;
bool writeDataTmpTable2 = false;
bool writeDataTable2 = false;
bool SEARCH = false;
bool SEARCH_BATCH = false;
bool VERIFY = false;
bool FULL_BUCKETS = false;
// bool ENABLE_DETAILED_METRICS = false;
// bool METRICS_OUTPUT_JSON = false;
// bool METRICS_OUTPUT_CSV = false;

char *SOURCE = "ssd-raid0";
char *DESTINATION = "ssd-raid0";
char user[256];

uint8_t key[32];

size_t BATCH_SIZE = 1024;
size_t PREFIX_SEARCH_SIZE = 1;
size_t WRITE_BATCH_SIZE_MB = 1024; // Default write batch size
size_t READ_BATCH_SIZE = 1;

double matching_factor = 1.0;

// --- state & counters ---
// size_t zero_nonce_count = 0;
// size_t count_condition_met = 0;
// size_t count_condition_not_met = 0;

uint8_t plot_id[32];
uint8_t hashed_key[32];

// Bucket pointers for both approaches
Bucket *buckets;
Bucket *buckets_phase2;

// Out-of-memory approach buckets
BucketTable2 *buckets2;
BucketTable2 *buckets2_phase2;

// In-memory merge approach structures
BucketTable2 *buckets_table2;
MemoTable2Record *table2;

// Timing variables for performance measurement
double start_time = 0.0;
double elapsed_time_io_total = 0.0;
double elapsed_time_shuffle_total = 0.0;
double elapsed_time_hash_total = 0.0;
double elapsed_time_hash2_total = 0.0;

// I/O tracking variables
unsigned long long total_bytes_written = 0;
unsigned long long total_bytes_read = 0;

// Initialize global metrics structure
// GlobalMetrics global_metrics = {0};
