#include <stdbool.h>
#include "globals.h"

int K = 24; // Default exponent

unsigned long long num_records_in_bucket = 1;
unsigned long long num_buckets = 1;
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
bool SEARCH = false;
bool SEARCH_BATCH = false;
bool VERIFY = false;
bool FULL_BUCKETS = false;

size_t BATCH_SIZE = 1024; 
size_t PREFIX_SEARCH_SIZE = 1;

Bucket *buckets;
BucketTable2 *buckets2;