#ifndef VAULTX_H
#define VAULTX_H

#ifdef __linux__
#include <linux/fs.h> // Provides `syncfs` on Linux
#endif

#ifdef __cplusplus
// Your C++-specific code here
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif

#include "globals.h"
#include "search.h"
#include "io.h"
#include "table2.h"

void print_usage(char *prog_name);
off_t getBucketIndex(const uint8_t *hash, size_t prefix_size);
unsigned long long byteArrayToLongLong(const uint8_t *byteArray, size_t length);
void generateBlake3(uint8_t *record_hash, MemoRecord *record, unsigned long long seed);
size_t writeBucketToDiskSequential(const Bucket *bucket, FILE *fd);
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex);
char *concat_strings(const char *str1, const char *str2);
bool is_nonce_nonzero(const uint8_t *nonce, size_t nonce_size);
size_t count_zero_memo_records(const char *filename);
long get_file_size(const char *filename);
size_t process_memo_records(const char *filename, const size_t BATCH_SIZE);
uint8_t *convert_string_to_uint8_array(const char *SEARCH_STRING);
uint8_t *hexStringToByteArray(const char *hexString);
uint64_t largest_power_of_two_less_than(uint64_t number);
int rename_file(const char *old_name, const char *new_name);
void remove_file(const char *fileName);
int move_file_overwrite(const char *source_path, const char *destination_path);

#endif // VAULTX_H