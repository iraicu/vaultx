#include "globals.h"
#include <limits.h>
#include <stdbool.h>

static unsigned long long get_system_memory_mb(void) {
#ifdef __APPLE__
  uint64_t memsize = 0;
  size_t len = sizeof(memsize);
  int mib[2] = {CTL_HW, HW_MEMSIZE};

  if (sysctl(mib, 2, &memsize, &len, NULL, 0) == -1) {
    return 0;
  }
  return memsize / (1024ULL * 1024ULL);
#elif defined(__linux__)
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages < 0 || page_size < 0) {
    return 0;
  }
  return (unsigned long long)pages * (unsigned long long)page_size /
         (1024ULL * 1024ULL);
#else
  return 0;
#endif
}

static int get_system_cores(void) {
#ifdef __APPLE__
  int mib[2];
  int cores = 0;
  size_t len = sizeof(cores);

  mib[0] = CTL_HW;
  mib[1] = HW_NCPU;

  if (sysctl(mib, 2, &cores, &len, NULL, 0) == -1) {
    return -1;
  }
  return cores;
#elif defined(__linux__)
  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs < 1) {
    return -1;
  }
  return (int)nprocs;
#else
  return -1;
#endif
}

void init_system_defaults(void) {
  unsigned long long mem_mb = get_system_memory_mb();
  if (mem_mb > 0) {
    MEMORY_LIMIT_MB =
        (mem_mb > (unsigned long long)INT_MAX) ? INT_MAX : (int)mem_mb;
  }

  int cores = get_system_cores();
  if (cores > 0) {
    num_threads = cores;
  }
}

// Defaults
int K = 27;
int current_file = 1;
int MERGE_APPROACH = 0;
int TOTAL_FILES = 2;
int BATCH_MEMORY_MB = 256;
int MEMORY_LIMIT_MB = 1024;
int num_threads = 1;
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
