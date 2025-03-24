#include <stdbool.h>
#include "globals.h"

unsigned long long num_records_in_bucket = 1;
unsigned long long num_buckets = 1;
unsigned long long rounds = 1; // filesize : memorysize 

bool DEBUG = false;
bool BENCHMARK = false; 
bool CIRCULAR_ARRAY = false;
bool HASHGEN = true;
bool MEMORY_WRITE = true;
bool writeData = false; 
bool writeDataFinal = false;
bool TABLE2 = true; 
bool SEARCH = false;
bool SEARCH_BATCH = false;
bool VERIFY = false;

size_t BATCH_SIZE = 1024; 
size_t PREFIX_SEARCH_SIZE = 1;