#include "globals.h"
#include <limits.h>
#include <stdbool.h>

// Defaults
int K = 27;
int current_file = 1;
int MERGE_APPROACH = 0;
int TOTAL_FILES = 2;
int BATCH_MEMORY_MB = 256;
int MEMORY_LIMIT_MB = 4096;
int num_threads = 1;
int MERGE_IO_THREADS = 1;
int DIFFICULTY = 0;
int PRINT_BUCKETS_COUNT = 0;
int PRINT_RECORDS_COUNT = 0;

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
bool VERIFY_ONLY = false;
bool FULL_BUCKETS = false;
bool PRINT_BUCKETS = false;
bool PRINT_RECORDS = false;
bool PREVIOUS_SEARCH = false;
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

// Merge phase timing summaries (set by merge.c)
double merge_read_time = 0.0;
double merge_write_time = 0.0;
double merge_compute_time = 0.0;
double merge_total_time = 0.0;

// I/O tracking variables
unsigned long long total_bytes_written = 0;
unsigned long long total_bytes_read = 0;

// Initialize global metrics structure
// GlobalMetrics global_metrics = {0};

void init_system_defaults(void) {
  int max_threads = omp_get_max_threads();
  if (max_threads < 1) {
    max_threads = 1;
  }
  num_threads = max_threads;
  MERGE_IO_THREADS = max_threads;
  if (MEMORY_LIMIT_MB < 1) {
    MEMORY_LIMIT_MB = 4096;
  }
}

void set_global_num_threads(int threads) {
  if (threads < 1) {
    threads = 1;
  }
  num_threads = threads;
  omp_set_num_threads(threads);
}

void set_global_memory_limit_mb(unsigned long long mb) {
  if (mb == 0) {
    return;
  }
  if (mb > (unsigned long long)INT_MAX) {
    MEMORY_LIMIT_MB = INT_MAX;
  } else {
    MEMORY_LIMIT_MB = (int)mb;
  }
}
