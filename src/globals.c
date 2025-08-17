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

unsigned long long total_nonces;
unsigned long long num_records_in_bucket = 1;
unsigned long long num_records_in_shuffled_bucket = 1;
unsigned long long total_buckets = 1;
unsigned long long rounds = 1; // filesize : memorysize
unsigned long long num_buckets_to_read = 1;
unsigned long long full_buckets_global = 0;

bool DEBUG = false;
bool BENCHMARK = false;
bool CIRCULAR_ARRAY = false;
bool HASHGEN = true;
bool MEMORY_WRITE = true;
bool writeData = false;
bool writeDataFinal = false;
bool writeDataTable2 = false;
bool writeDataTable2Tmp = false;
bool SEARCH = false;
bool SEARCH_BATCH = false;
bool VERIFY = false;
bool FULL_BUCKETS = false;

char* SOURCE = 'ssd-raid0';
char* DESTINATION = 'ssd-raid0';
char user[256];

uint8_t key[32];

size_t BATCH_SIZE = 1024;
size_t PREFIX_SEARCH_SIZE = 1;

Bucket* buckets;
BucketTable2* buckets2;
Bucket* buckets_phase2;
BucketTable2* buckets_table2;
BucketTable2* buckets2_phase2;

MemoTable2Record* table2;