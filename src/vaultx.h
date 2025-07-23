#ifndef VAULTX_H
#define VAULTX_H
#define _GNU_SOURCE

#ifdef __linux__
#include <linux/fs.h> // Provides `syncfs` on Linux
#endif

#ifdef __cplusplus
// Your C++-specific code here
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#endif

#include "crypto.h"
#include "globals.h"
#include "io.h"
#include "search.h"
#include "shuffle.h"
#include "sort.h"
#include "table1.h"
#include "table2.h"
#include "utils.h"
#include <ctype.h>
#include <dirent.h>
#include <numa.h>
#include <numaif.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void print_usage(char* prog_name);
off_t getBucketIndex(const uint8_t* hash);
unsigned long long byteArrayToLongLong(const uint8_t* byteArray, size_t length);
char* byteArrayToHexString(const unsigned char* bytes, size_t len);
size_t writeBucketToDiskSequential(const Bucket* bucket, FILE* fd);
void insert_record(Bucket* buckets, MemoRecord* record, size_t bucketIndex);
char* concat_strings(const char* str1, const char* str2);
bool is_nonce_nonzero(const uint8_t* nonce, size_t nonce_size);
size_t count_zero_memo_records(const char* filename);
long get_file_size(const char* filename);
size_t process_memo_records(const char* filename, const size_t BATCH_SIZE);
uint8_t* convert_string_to_uint8_array(const char* SEARCH_STRING);
uint8_t* hexStringToByteArray(const char* hexString);
uint64_t largest_power_of_two_less_than(uint64_t number);
int rename_file(const char* old_name, const char* new_name);
void remove_file(const char* fileName);
int move_file_overwrite(const char* source_path, const char* destination_path);

#endif // VAULTX_H