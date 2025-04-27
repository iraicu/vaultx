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

#include <execinfo.h>
#include <signal.h>
#include <limits.h> // For UINT64_MAX

#include <inttypes.h>

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

int K = 24; // Default exponent

unsigned long long num_buckets = 1;
unsigned long long num_records_in_bucket = 1;
unsigned long long rounds = 1;

size_t BATCH_SIZE = 1024;

unsigned long long full_buckets_global = 0;

bool VERIFY = false;
bool DEBUG = false;
bool FULL_BUCKETS = false;
bool writeData = false;
bool writeDataFinal = false;
bool writeDataTable2 = false;
bool MEMORY_WRITE = true;
bool CIRCULAR_ARRAY = false;
bool BENCHMARK = false;
bool HASHGEN = true;
bool SEARCH = false;
bool SEARCH_BATCH = false;
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

// Structure to hold a record with nonce
typedef struct
{
    uint8_t nonce1[NONCE_SIZE]; // Nonce to store the seed
    uint8_t nonce2[NONCE_SIZE]; // Nonce to store the seed
} MemoRecord2;

typedef struct
{
    MemoRecord *records;
    size_t count;       // Number of records in the bucket
    size_t count_waste; // Number of records generated but not stored
    bool full;          // Number of records in the bucket
    size_t flush;       // Number of flushes of bucket
} Bucket;

typedef struct
{
    MemoRecord2 *records;
    size_t count;       // Number of records in the bucket
    size_t count_waste; // Number of records generated but not stored
    bool full;          // Number of records in the bucket
    size_t flush;       // Number of flushes of bucket
} Bucket2;

Bucket *buckets;
Bucket2 *buckets2;

// Function to display usage information
void print_usage(char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -a [xtask|task|for|tbb]   Select parallelization approach (default: for)\n");
    printf("  -t NUM                    Number of threads to use (default: number of available cores)\n");
    printf("  -K NUM                    Exponent K to compute iterations as 2^K (default: 4)\n");
    printf("  -m NUM                    Memory size in MB (default: 1)\n");
    printf("  -f NAME                   Temporary file name raw\n");
    printf("  -g NAME                   Temporary file name table1\n");
    printf("  -j NAME                   Final file name table2\n");
    printf("  -b NUM                    Batch size (default: 1024)\n");
    printf("  -h, --help                Display this help message\n");
    printf("\nExample:\n");
    printf("  %s -a task -t 8 -K 20 -m 1024 -f output.dat\n", prog_name);
}

// Function to compute the bucket index based on hash prefix
off_t getBucketIndex(const uint8_t *hash, size_t prefix_size)
{
    off_t index = 0;
    for (size_t i = 0; i < prefix_size && i < HASH_SIZE; i++)
    {
        index = (index << 8) | hash[i];
    }
    return index;
}

// Function to convert bytes to unsigned long long
unsigned long long byteArrayToLongLong(const uint8_t *byteArray, size_t length)
{
    unsigned long long result = 0;
    for (size_t i = 0; i < length; ++i)
    {
        result = (result << 8) | (unsigned long long)byteArray[i];
    }
    return result;
}

// Function to generate Blake3 hash
void generateBlake3(uint8_t *record_hash, MemoRecord *record, unsigned long long seed)
{
    // Ensure that the pointers are valid
    if (record_hash == NULL || record->nonce == NULL)
    {
        fprintf(stderr, "Error: NULL pointer passed to generateBlake3.\n");
        return;
    }

    // Store seed into the nonce
    memcpy(record->nonce, &seed, NONCE_SIZE);

    // Generate Blake3 hash
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, record_hash, HASH_SIZE);
}

// Function to generate Blake3 hash
void generate2Blake3(uint8_t *record_hash, MemoRecord2 *record, unsigned long long nonce1, unsigned long long nonce2)
{
    // Ensure that the pointers are valid
    if (record_hash == NULL || record->nonce1 == NULL || record->nonce2 == NULL)
    {
        fprintf(stderr, "Error: NULL pointer passed to generateBlake3.\n");
        return;
    }

    // Store seed into the nonce
    memcpy(record->nonce1, &nonce1, NONCE_SIZE);
    memcpy(record->nonce2, &nonce2, NONCE_SIZE);

    // Generate Blake3 hash
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record->nonce1, NONCE_SIZE);
    blake3_hasher_update(&hasher, record->nonce2, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, record_hash, HASH_SIZE);
}

// Comparison function for qsort(), comparing the hash fields.
int compare_memo_all_record(const void *a, const void *b)
{
    const MemoAllRecord *recA = (const MemoAllRecord *)a;
    const MemoAllRecord *recB = (const MemoAllRecord *)b;
    return memcmp(recA->hash, recB->hash, HASH_SIZE);
}

/**
 * sort_bucket_records:
 *   - Takes an array of unsorted MemoRecord entries (with only nonce values).
 *   - Internally allocates an array of MemoAllRecord, copies the nonces,
 *     computes their Blake3 hashes, and sorts them based on the hash values.
 *   - Extracts and returns a new array of MemoRecord that contains only the sorted nonces.
 *
 * @param unsorted      Pointer to an array of MemoRecord.
 * @param total_records Total number of records in the bucket.
 * @return Pointer to a newly allocated array of MemoRecord with sorted nonce values.
 */
MemoRecord *sort_bucket_records(const MemoRecord *unsorted, size_t total_records)
{
    // Allocate a temporary array of MemoAllRecord.
    MemoAllRecord *all_records = malloc(total_records * sizeof(MemoAllRecord));
    if (!all_records)
    {
        perror("Error allocating memory for MemoAllRecord array");
        exit(EXIT_FAILURE);
    }

    // Populate all_records by copying the nonce from each unsorted record
    // and computing its hash.
    for (size_t i = 0; i < total_records; i++)
    {
        memcpy(all_records[i].nonce, unsorted[i].nonce, NONCE_SIZE);
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, all_records[i].nonce, NONCE_SIZE);
        blake3_hasher_finalize(&hasher, all_records[i].hash, HASH_SIZE);
    }

    // Sort the MemoAllRecord array based on the computed hash values.
    qsort(all_records, total_records, sizeof(MemoAllRecord), compare_memo_all_record);

    // Allocate an array to hold the sorted nonces.
    MemoRecord *sorted_nonces = malloc(total_records * sizeof(MemoRecord));
    if (!sorted_nonces)
    {
        perror("Error allocating memory for sorted nonces");
        free(all_records);
        exit(EXIT_FAILURE);
    }

    // Extract the nonce values from the sorted MemoAllRecord array.
    for (size_t i = 0; i < total_records; i++)
    {
        memcpy(sorted_nonces[i].nonce, all_records[i].nonce, NONCE_SIZE);
    }

    // Free the temporary MemoAllRecord array.
    free(all_records);

    return sorted_nonces;
}

void sort_bucket_records_inplace(MemoRecord *records, size_t total_records)
{
    // Build an auxiliary array of (nonce, hash) pairs
    MemoAllRecord *all_records = malloc(total_records * sizeof(MemoAllRecord));
    if (!all_records)
    {
        perror("Error allocating memory for MemoAllRecord array");
        exit(EXIT_FAILURE);
    }

    // Populate it
    for (size_t i = 0; i < total_records; i++)
    {
        memcpy(all_records[i].nonce, records[i].nonce, NONCE_SIZE);
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, all_records[i].nonce, NONCE_SIZE);
        blake3_hasher_finalize(&hasher, all_records[i].hash, HASH_SIZE);
    }

    // Sort by hash
    qsort(all_records,
          total_records,
          sizeof(MemoAllRecord),
          compare_memo_all_record);

    // Copy sorted nonces back into the original array
    for (size_t i = 0; i < total_records; i++)
    {
        memcpy(records[i].nonce, all_records[i].nonce, NONCE_SIZE);
    }

    // Clean up
    free(all_records);
}

// Function to write a bucket of records to disk sequentially
size_t writeBucketToDiskSequential(const Bucket *bucket, FILE *fd)
{
    // printf("num_records_in_bucket=%llu sizeof(MemoRecord)=%d\n",num_records_in_bucket,sizeof(MemoRecord));

    // MemoRecord *sorted_nonces = sort_bucket_records(bucket->records, num_records_in_bucket);

    size_t elementsWritten = fwrite(bucket->records, sizeof(MemoRecord), num_records_in_bucket, fd);
    // size_t elementsWritten = fwrite(sorted_nonces, sizeof(MemoRecord), num_records_in_bucket, fd);
    if (elementsWritten != num_records_in_bucket)
    {
        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                elementsWritten, num_records_in_bucket);
        fclose(fd);
        exit(EXIT_FAILURE);
    }
    return elementsWritten * sizeof(MemoRecord);
}

// Function to insert a record into a bucket
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex)
{
    if (bucketIndex >= num_buckets)
    {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, num_buckets - 1);
        return;
    }

    Bucket *bucket = &buckets[bucketIndex];
    size_t idx;

// Atomically capture the current count and increment it
#pragma omp atomic capture
    {
        idx = bucket->count;
        bucket->count++;
    }

    // Check if there's room in the bucket
    if (idx < num_records_in_bucket)
    {
        memcpy(bucket->records[idx].nonce, record->nonce, NONCE_SIZE);
    }
    else
    {
        // Ensure count doesn't exceed the maximum allowed
        bucket->count = num_records_in_bucket;
        if (!bucket->full)
        {
#pragma omp atomic
            full_buckets_global++;
            bucket->full = true;
        }
        bucket->count_waste++;
        // Overflow handling can be added here if necessary.
    }
}

// Function to insert a record into a bucket
void insert_record2(Bucket2 *buckets2, MemoRecord2 *record, size_t bucketIndex)
{
    if (bucketIndex >= num_buckets)
    {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, num_buckets - 1);
        return;
    }

    Bucket2 *bucket = &buckets2[bucketIndex];
    size_t idx;

// Atomically capture the current count and increment it
#pragma omp atomic capture
    {
        idx = bucket->count;
        bucket->count++;
    }

    // Check if there's room in the bucket
    if (idx < num_records_in_bucket)
    {
        memcpy(bucket->records[idx].nonce1, record->nonce1, NONCE_SIZE);
        memcpy(bucket->records[idx].nonce2, record->nonce2, NONCE_SIZE);
    }
    else
    {
        // Ensure count doesn't exceed the maximum allowed
        bucket->count = num_records_in_bucket;
        if (!bucket->full)
        {
#pragma omp atomic
            full_buckets_global++;
            bucket->full = true;
        }
        bucket->count_waste++;
        // Overflow handling can be added here if necessary.
    }
}

// Function to concatenate two strings and return the result
char *concat_strings(const char *str1, const char *str2)
{
    // Check for NULL pointers
    if (str1 == NULL || str2 == NULL)
    {
        fprintf(stderr, "Error: NULL string passed to concat_strings.\n");
        return NULL;
    }

    // Calculate the lengths of the input strings
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    // Allocate memory for the concatenated string (+1 for the null terminator)
    char *result = (char *)malloc(len1 + len2 + 1);
    if (result == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed in concat_strings.\n");
        return NULL;
    }

    // Copy the first string into the result
    strcpy(result, str1);

    // Append the second string to the result
    strcat(result, str2);

    return result;
}

// Function to check if a nonce is non-zero
bool is_nonce_nonzero(const uint8_t *nonce, size_t nonce_size)
{
    // Check for NULL pointer
    if (nonce == NULL)
    {
        // Handle error as needed
        return false;
    }

    // Iterate over each byte of the nonce
    for (size_t i = 0; i < nonce_size; ++i)
    {
        if (nonce[i] != 0)
        {
            // Found a non-zero byte
            return true;
        }
    }

    // All bytes are zero
    return false;
}

// Function to count zero-value MemoRecords in a binary file
size_t count_zero_memo_records(const char *filename)
{
    const size_t BATCH_SIZE = 1000000; // 1 million MemoRecords per batch
    MemoRecord *buffer = NULL;
    size_t total_zero_records = 0;
    size_t total_nonzero_records = 0;
    size_t records_read;
    FILE *file = NULL;

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {

        printf("Error opening file %s (#1)\n", filename);
        return 0;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(BATCH_SIZE * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        // Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            // Check if the MemoRecord's nonce is all zeros
            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                ++total_nonzero_records;
            }
            else
            {
                ++total_zero_records;
            }
        }
    }

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of zero-value MemoRecords
    printf("total_zero_records=%zu total_nonzero_records=%zu efficiency=%.2f%%\n", total_zero_records, total_nonzero_records, total_nonzero_records * 100.0 / (total_zero_records + total_nonzero_records));

    return total_zero_records;
}

long get_file_size(const char *filename)
{
    FILE *file = fopen(filename, "rb"); // Open the file in binary mode
    long size;

    if (file == NULL)
    {
        printf("Error opening file %s (#2)\n", filename);

        perror("Error opening file");
        return -1;
    }

    // Move the file pointer to the end of the file
    if (fseek(file, 0, SEEK_END) != 0)
    {
        perror("Error seeking to end of file");
        fclose(file);
        return -1;
    }

    // Get the current position in the file, which is the size
    size = ftell(file);
    if (size == -1L)
    {
        perror("Error getting file position");
        fclose(file);
        return -1;
    }

    fclose(file);
    return size;
}

uint64_t compute_hash_hamming_distance(const uint8_t *hash_output,
                                       const uint8_t *prev_hash,
                                       size_t hash_size)
{
    uint64_t distance = 0;
    for (size_t i = 0; i < hash_size; i++)
    {
        uint8_t diff = hash_output[i] ^ prev_hash[i];
        // Count bits set in diff (Brian Kernighan's algorithm)
        while (diff)
        {
            distance++;
            diff &= (diff - 1);
        }
    }
    return distance;
}

uint64_t compute_hash_match(const uint8_t *hash1, const uint8_t *hash2, size_t hash_size)
{
    uint64_t match = 0;
    for (size_t i = 0; i < hash_size; i++)
    {
        // XOR gives 0 where bits are the same, so invert to mark matching bits as 1.
        uint8_t equal = ~(hash1[i] ^ hash2[i]);
        // Count the bits that are 1 (i.e. bits that match) using Brian Kernighan's algorithm.
        while (equal)
        {
            match++;
            equal &= (equal - 1);
        }
    }
    return match;
}

uint64_t compute_hash_match_leading(const uint8_t *hash1, const uint8_t *hash2, size_t hash_size)
{
    uint64_t match = 0;

    // Iterate over each byte in the hash
    for (size_t i = 0; i < hash_size; i++)
    {
        // Check each bit starting with the most significant bit
        for (int bit = 7; bit >= 0; bit--)
        {
            uint8_t mask = 1 << bit;
            // If the bits match, increase the counter; if not, return immediately.
            if ((hash1[i] & mask) == (hash2[i] & mask))
                match++;
            else
                return match;
        }
    }
    return match;
}

uint64_t compute_hash_distance(const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size)
{
    // Ensure there are at least 8 bytes in the hash
    if (hash_size < 8)
    {
        fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
        return 0;
    }

    // Convert the first 8 bytes of each hash to a uint64_t
    uint64_t current = byteArrayToLongLong(hash_output, 8);
    uint64_t previous = byteArrayToLongLong(prev_hash, 8);

    // Subtract the previous hash value from the current one and return the result
    return previous - current;
}

int print_table2_entry(const uint8_t *nonce_output, const uint8_t *prev_nonce, const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size)
{
    // Ensure there are at least 8 bytes in the hash
    if (hash_size < 8)
    {
        fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
        return -1;
    }

    // Convert the first 8 bytes of each hash to a uint64_t
    // uint64_t current = byteArrayToLongLong(hash_output, 8);
    // uint64_t previous = byteArrayToLongLong(prev_hash, 8);
    // uint64_t distance = previous - current;

    // Print debugging information
    fprintf(stderr, "Debug Info:\n");

    // Print the first 8 bytes of hash_output
    fprintf(stderr, "hash_output (first 8 bytes): ");
    for (size_t i = 0; i < 8; i++)
    {
        fprintf(stderr, "%02X ", hash_output[i]);
    }
    fprintf(stderr, "\n");

    // Print the first 8 bytes of prev_hash
    fprintf(stderr, "prev_hash (first 8 bytes): ");
    for (size_t i = 0; i < 8; i++)
    {
        fprintf(stderr, "%02X ", prev_hash[i]);
    }
    fprintf(stderr, "\n");

    // Print the computed 64-bit values and the difference
    // fprintf(stderr, "current: %" PRIu64 "\n", current);
    // fprintf(stderr, "previous: %" PRIu64 "\n", previous);
    // fprintf(stderr, "current - previous: %" PRIu64 "\n", distance);

    uint8_t hash_table2[hash_size];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, prev_nonce, NONCE_SIZE);
    blake3_hasher_update(&hasher, nonce_output, NONCE_SIZE);

    blake3_hasher_finalize(&hasher, hash_table2, HASH_SIZE);

    // Print the first 8 bytes of hash_table2
    fprintf(stderr, "hash_table2 (first 8 bytes): ");
    for (size_t i = 0; i < 8; i++)
    {
        fprintf(stderr, "%02X ", hash_table2[i]);
    }
    fprintf(stderr, "\n");

    return 0;
}

uint64_t compute_hash_distance_debug(const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size)
{
    // Ensure there are at least 8 bytes in the hash
    if (hash_size < 8)
    {
        fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
        return 0;
    }

    // Convert the first 8 bytes of each hash to a uint64_t
    uint64_t current = byteArrayToLongLong(hash_output, 8);
    uint64_t previous = byteArrayToLongLong(prev_hash, 8);
    uint64_t distance = previous - current;

    // Print debugging information
    fprintf(stderr, "Debug Info:\n");

    // Print the first 8 bytes of hash_output
    fprintf(stderr, "hash_output (first 8 bytes): ");
    for (size_t i = 0; i < 8; i++)
    {
        fprintf(stderr, "%02X ", hash_output[i]);
    }
    fprintf(stderr, "\n");

    // Print the first 8 bytes of prev_hash
    fprintf(stderr, "prev_hash (first 8 bytes): ");
    for (size_t i = 0; i < 8; i++)
    {
        fprintf(stderr, "%02X ", prev_hash[i]);
    }
    fprintf(stderr, "\n");

    // Print the computed 64-bit values and the difference
    fprintf(stderr, "current: %" PRIu64 "\n", current);
    fprintf(stderr, "previous: %" PRIu64 "\n", previous);
    fprintf(stderr, "current - previous: %" PRIu64 "\n", distance);

    return distance;
}

// Returns true if the first 'prefix_bits' of hash1 and hash2 are identical.
bool compute_hash_similarity(const uint8_t *hash1, const uint8_t *hash2, size_t hash_size, int prefix_bits)
{
    // Determine how many full bytes are covered by prefix_bits.
    int full_bytes = prefix_bits / 8;
    // And how many extra bits remain.
    int extra_bits = prefix_bits % 8;

    // Compare all the full bytes.
    for (size_t i = 0; i < (size_t)full_bytes; i++)
    {
        if (hash1[i] != hash2[i])
            return false;
    }

    // If there are extra bits, compare only the high bits of the next byte.
    if (extra_bits > 0 && full_bytes < (int)hash_size)
    {
        // Create a mask to extract the top 'extra_bits' bits.
        uint8_t mask = 0xFF << (8 - extra_bits);
        if ((hash1[full_bytes] & mask) != (hash2[full_bytes] & mask))
            return false;
    }

    return true;
}

uint64_t hash_pass_count_global = 0;

size_t process_memo_records(const char *filename, const size_t BATCH_SIZE)
{
    // const size_t BATCH_SIZE = 1000000; // 1 million MemoRecords per batch
    MemoRecord *buffer = NULL;
    size_t total_records = 0;
    size_t zero_nonce_count = 0;
    size_t full_buckets = 0;
    bool bucket_not_full = false;
    size_t records_read;
    FILE *file = NULL;
    uint8_t prev_hash[HASH_SIZE] = {0};   // Initialize previous hash prefix to zero
    uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    size_t count_condition_met = 0;       // Counter for records meeting the condition
    size_t count_condition_not_met = 0;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    printf("num_records_in_bucket=%llu\n", num_records_in_bucket);
    printf("BATCH_SIZE=%lu\n", BATCH_SIZE);
    printf("filesize=%ld\n", filesize);

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return 0;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(BATCH_SIZE * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        double start_time_verify = omp_get_wtime();
        double end_time_verify = omp_get_wtime();
        bucket_not_full = false;
        // uint64_t distance = 0;
        // uint64_t HASH_SPACE = 1 << K;
        // uint64_t M = 1 << 64;
        // uint64_t M = UINT64_MAX;
        // uint64_t expected_distance = M/(HASH_SPACE+1);
        // uint64_t expected_distance = 1 << (64 - K + 1);
        // printf("expected_distance=%llu\n",expected_distance);
        // uint64_t min_distance = UINT64_MAX;
        // uint64_t max_distance = 0;
        // uint64_t total_distance = 0;
        uint64_t count = 0;
        // bool hash_pass = false;
        // uint64_t hash_pass_count = 0;
        // printf("records_read=%ld\n",records_read);
        //  Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            ++total_records;

            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                uint8_t hash_output[HASH_SIZE];

                // Compute Blake3 hash of the nonce
                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, buffer[i].nonce, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);

                // Compare the first PREFIX_SIZE bytes of the current hash to the previous hash prefix
                if (memcmp(hash_output, prev_hash, PREFIX_SIZE) >= 0)
                {
                    // Current hash's first PREFIX_SIZE bytes are equal to or greater than previous
                    ++count_condition_met;
                }
                else
                {
                    ++count_condition_not_met;

                    if (DEBUG)
                    {
                        // Print previous hash and nonce, and current hash and nonce
                        printf("Condition not met at record %zu:\n", total_records);
                        printf("Previous nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", prev_nonce[n]);
                        printf("\n");
                        printf("Previous hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", prev_hash[n]);
                        printf("\n");

                        printf("Current nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce[n]);
                        printf("\n");
                        printf("Current hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", hash_output[n]);
                        printf("\n");
                    }
                }

                /*            // Call the function and capture the Hamming distance.
                            distance = compute_hash_hamming_distance(hash_output, prev_hash, HASH_SIZE);

                                    // Update min and max values.
                    if (distance < min_distance) {
                        min_distance = distance;
                    }
                    if (distance > max_distance) {
                        max_distance = distance;
                    }*/

                // hash_pass = compute_hash_similarity(prev_hash, hash_output, HASH_SIZE, 25);
                /*distance = compute_hash_distance(prev_hash, hash_output, HASH_SIZE);
                if (distance <= expected_distance)
                {
                    hash_pass_count++;
                    }*/

                /*
                             printf("Record %zu:\n", total_records);
                             printf("Previous nonce: ");
                             for (size_t n = 0; n < NONCE_SIZE; ++n)
                                 printf("%02X", prev_nonce[n]);
                             printf("\n");
                             printf("Previous hash prefix: ");
                             for (size_t n = 0; n < HASH_SIZE; ++n)
                                 printf("%02X", prev_hash[n]);
                             printf("\n");

                             printf("Current nonce: ");
                             for (size_t n = 0; n < NONCE_SIZE; ++n)
                                 printf("%02X", buffer[i].nonce[n]);
                             printf("\n");
                             printf("Current hash prefix: ");
                             for (size_t n = 0; n < HASH_SIZE; ++n)
                                 printf("%02X", hash_output[n]);
                             printf("\n");
                             */

                // Update running total and count.
                // total_distance += distance;
                count++;

                //	printf("count=%llu distance=%llu min_distance=%llu max_distance=%llu total_distance=%llu hash_pass_count=%llu\n",count, distance, min_distance, max_distance, total_distance,hash_pass_count);
                // printf("count=%llu hash_pass_count=%llu\n",count,hash_pass_count);
                //  Update the previous hash prefix and nonce
                memcpy(prev_hash, hash_output, HASH_SIZE);
                memcpy(prev_nonce, buffer[i].nonce, NONCE_SIZE);
            }
            else
            {
                ++zero_nonce_count;
                bucket_not_full = true;
                // Optionally, handle zero nonces here
            }
        }
        if (bucket_not_full == false)
            full_buckets++;
        end_time_verify = omp_get_wtime();

        // Calculate average distance.
        // double average_distance = (count > 0) ? (double)total_distance / count : 0.0;

        // Print summary of metrics.
        // printf("Min distance: %llu\n", min_distance);
        // printf("Max distance: %llu\n", max_distance);
        // printf("Average distance: %.2f\n", average_distance);
        /*hash_pass_count_global += hash_pass_count;
        if (total_records%(1024*1024) == 0)
        {
            printf("Hashed passed: %llu %llu %.2f\n", hash_pass_count_global,total_records,hash_pass_count_global*100.0/total_records);
            }*/

        double elapsed_time_verify = end_time_verify - start_time_verify;
        double elapsed_time = omp_get_wtime() - start_time;

        // Calculate throughput (hashes per second)
        double throughput = (BATCH_SIZE * sizeof(MemoRecord) / elapsed_time_verify) / (1024 * 1024);
        if (total_records % (1024 * 1024) == 0)
            printf("[%.2f] Verify %.2f%%: %.2f MB/s\n", elapsed_time, total_records * sizeof(MemoRecord) * 100.0 / filesize, throughput);
    }

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    printf("sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu storage_efficiency=%.2f bucket_efficiency=%.2f\n",
           count_condition_met, count_condition_not_met, zero_nonce_count, total_records, full_buckets, count_condition_met * 100.0 / total_records, full_buckets * 100.0 / (num_buckets));

    return count_condition_met;
}

size_t process_memo_records_table2_old(const char *filename, const size_t BATCH_SIZE)
{
    // const size_t BATCH_SIZE = 1000000; // 1 million MemoRecords per batch
    MemoRecord2 *buffer = NULL;
    size_t total_records = 0;
    size_t zero_nonce_count = 0;
    size_t full_buckets = 0;
    bool bucket_not_full = false;
    size_t records_read;
    FILE *file = NULL;
    uint8_t prev_hash[HASH_SIZE] = {0};    // Initialize previous hash prefix to zero
    uint8_t prev_nonce1[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    uint8_t prev_nonce2[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    size_t count_condition_met = 0;        // Counter for records meeting the condition
    size_t count_condition_not_met = 0;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    printf("num_records_in_bucket=%llu\n", num_records_in_bucket);
    printf("BATCH_SIZE=%lu\n", BATCH_SIZE);
    printf("filesize=%ld\n", filesize);

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return 0;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord2 *)malloc(BATCH_SIZE * sizeof(MemoRecord2));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord2), BATCH_SIZE, file)) > 0)
    {
        double start_time_verify = omp_get_wtime();
        double end_time_verify = omp_get_wtime();
        bucket_not_full = false;
        // uint64_t distance = 0;
        // uint64_t expected_distance = 1 << (64 - K + 1);
        // uint64_t min_distance = UINT64_MAX;
        // uint64_t max_distance = 0;
        // uint64_t total_distance = 0;
        uint64_t count = 0;
        // bool hash_pass = false;
        // uint64_t hash_pass_count = 0;

        // Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            ++total_records;

            if (is_nonce_nonzero(buffer[i].nonce1, NONCE_SIZE) && is_nonce_nonzero(buffer[i].nonce2, NONCE_SIZE))
            {
                uint8_t hash_output[HASH_SIZE];

                // Compute Blake3 hash of the nonce
                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, buffer[i].nonce1, NONCE_SIZE);
                blake3_hasher_update(&hasher, buffer[i].nonce2, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);

                // Compare the first PREFIX_SIZE bytes of the current hash to the previous hash prefix
                if (memcmp(hash_output, prev_hash, PREFIX_SIZE) >= 0)
                {
                    // Current hash's first PREFIX_SIZE bytes are equal to or greater than previous
                    ++count_condition_met;
                }
                else
                {
                    ++count_condition_not_met;

                    if (DEBUG)
                    {
                        // Print previous hash and nonce, and current hash and nonce
                        printf("Condition not met at record %zu:\n", total_records);
                        printf("Previous nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", prev_nonce1[n]);
                        printf("\n");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", prev_nonce2[n]);
                        printf("\n");
                        printf("Previous hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", prev_hash[n]);
                        printf("\n");

                        printf("Current nonces: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce1[n]);
                        printf("\n");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce2[n]);
                        printf("\n");
                        printf("Current hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", hash_output[n]);
                        printf("\n");
                    }
                }

                count++;

                // Update the previous hash prefix and nonce
                memcpy(prev_hash, hash_output, HASH_SIZE);
                memcpy(prev_nonce1, buffer[i].nonce1, NONCE_SIZE);
                memcpy(prev_nonce2, buffer[i].nonce2, NONCE_SIZE);
            }
            else
            {
                ++zero_nonce_count;
                bucket_not_full = true;
                // Optionally, handle zero nonces here
            }
        }
        if (bucket_not_full == false)
            full_buckets++;
        end_time_verify = omp_get_wtime();

        double elapsed_time_verify = end_time_verify - start_time_verify;
        double elapsed_time = omp_get_wtime() - start_time;

        // Calculate throughput (hashes per second)
        double throughput = (BATCH_SIZE * sizeof(MemoRecord2) / elapsed_time_verify) / (1024 * 1024);
        if (total_records % (1024 * 1024) == 0)
            printf("[%.2f] Verify %.2f%%: %.2f MB/s\n", elapsed_time, total_records * sizeof(MemoRecord2) * 100.0 / filesize, throughput);
    }

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    printf("sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu storage_efficiency=%.2f bucket_efficiency=%.2f\n",
           count_condition_met, count_condition_not_met, zero_nonce_count, total_records, full_buckets, count_condition_met * 100.0 / total_records, full_buckets * 100.0 / (num_buckets));

    return count_condition_met;
}

size_t process_memo_records_table2(
    const char *filename,
    const size_t BATCH_SIZE)
{
    // --- open file & figure out how many records are in it ---
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Error opening file");
        return 0;
    }
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    if (filesize < 0)
    {
        perror("ftell failed");
        fclose(file);
        return 0;
    }
    size_t total_recs_in_file = filesize / sizeof(MemoRecord2);
    size_t num_buckets = (total_recs_in_file + BATCH_SIZE - 1) / BATCH_SIZE;
    rewind(file);

    // --- allocate one batch buffer ---
    MemoRecord2 *buffer = malloc(BATCH_SIZE * sizeof(MemoRecord2));
    if (!buffer)
    {
        fprintf(stderr, "Error: Unable to allocate buffer for %zu records\n", BATCH_SIZE);
        fclose(file);
        return 0;
    }

    // --- state & counters ---
    size_t total_records = 0;
    size_t zero_nonce_count = 0;
    size_t full_buckets = 0;
    size_t count_condition_met = 0;
    size_t count_condition_not_met = 0;

    uint8_t prev_hash[HASH_SIZE] = {0};
    uint8_t prev_nonce1[NONCE_SIZE] = {0};
    uint8_t prev_nonce2[NONCE_SIZE] = {0};

    // --- timing for progress updates ---
    double start_time = omp_get_wtime();
    double last_print_time = start_time;

    // --- read & process in one pass, printing progress every second ---
    for (size_t bucket = 0; bucket < num_buckets; bucket++)
    {
        bool bucket_not_full = false;
        size_t records_read = fread(buffer, sizeof(MemoRecord2), BATCH_SIZE, file);
        if (records_read == 0)
            break;

        for (size_t i = 0; i < records_read; i++)
        {
            ++total_records;

            if (is_nonce_nonzero(buffer[i].nonce1, NONCE_SIZE) &&
                is_nonce_nonzero(buffer[i].nonce2, NONCE_SIZE))
            {

                // compute the hash
                uint8_t hash_output[HASH_SIZE];
                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, buffer[i].nonce1, NONCE_SIZE);
                blake3_hasher_update(&hasher, buffer[i].nonce2, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);

                // compare prefix to previous
                if (memcmp(hash_output, prev_hash, PREFIX_SIZE) >= 0)
                {
                    ++count_condition_met;
                }
                else
                {
                    ++count_condition_not_met;
                    if (DEBUG)
                    {
                        // your debug prints...
                    }
                }

                // update previous
                memcpy(prev_hash, hash_output, HASH_SIZE);
                memcpy(prev_nonce1, buffer[i].nonce1, NONCE_SIZE);
                memcpy(prev_nonce2, buffer[i].nonce2, NONCE_SIZE);
            }
            else
            {
                ++zero_nonce_count;
                bucket_not_full = true;
            }

            // --- progress update every second ---
            double now = omp_get_wtime();
            if (now - last_print_time >= 1.0)
            {
                last_print_time = now;
                double elapsed = now - start_time;
                double pct = (double)total_records * 100.0 / (double)total_recs_in_file;
                double pct_met = (double)count_condition_met * 100.0 /
                                 (double)(count_condition_met + count_condition_not_met + zero_nonce_count);
                double pct_sorted = (double)count_condition_met * 100.0 /
                                    (double)(count_condition_met + count_condition_not_met);
                printf("[%.2f] Verify %.2f%%: Sorted %.2f%% : Storage Efficiency %.2f%%\n",
                       elapsed, pct, pct_sorted, pct_met);
            }
        }

        if (!bucket_not_full)
        {
            ++full_buckets;
        }
    }

    // ensure final 100% progress line
    double now = omp_get_wtime();
    double elapsed = now - start_time;
    double pct = (double)total_records * 100.0 / (double)total_recs_in_file;
    double pct_met = (double)count_condition_met * 100.0 /
                     (double)(count_condition_met + count_condition_not_met + zero_nonce_count);
    double pct_sorted = (double)count_condition_met * 100.0 /
                        (double)(count_condition_met + count_condition_not_met);

    // printf("Progress: 100.00%% (%zu/%zu)\n", total_records, total_recs_in_file);
    printf("[%.2f] Verify %.2f%%: Sorted %.2f%% : Storage Efficiency %.2f%%\n",
           elapsed, pct, pct_sorted, pct_met);

    // --- cleanup ---
    free(buffer);
    fclose(file);

    // --- final summary ---
    /*
    double storage_eff  = 100.0 * count_condition_met     / (double)total_records;
    double bucket_eff   = 100.0 * full_buckets            / (double)num_buckets;
    printf(
      "sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu "
      "storage_efficiency=%.2f bucket_efficiency=%.2f\n",
      count_condition_met,
      count_condition_not_met,
      zero_nonce_count,
      total_records,
      full_buckets,
      storage_eff,
      bucket_eff
    );
    */

    return count_condition_met;
}

size_t process_memo_records_debug(const char *filename, const size_t BATCH_SIZE)
{
    MemoRecord *buffer = NULL;
    size_t total_records = 0;
    size_t zero_nonce_count = 0;
    size_t full_buckets = 0;
    bool bucket_not_full = false;
    size_t records_read;
    FILE *file = NULL;

    // Now maintain two previous hashes and nonces.
    uint8_t prev_hash[HASH_SIZE] = {0};        // immediate previous hash
    uint8_t prev_prev_hash[HASH_SIZE] = {0};   // hash from two records back
    uint8_t prev_nonce[NONCE_SIZE] = {0};      // immediate previous nonce
    uint8_t prev_prev_nonce[NONCE_SIZE] = {0}; // nonce corresponding to prev_prev_hash

    size_t count_condition_met = 0; // Counter for records meeting the condition
    size_t count_condition_not_met = 0;

    long filesize = get_file_size(filename);
    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    printf("num_records_in_bucket=%llu\n", num_records_in_bucket);
    printf("BATCH_SIZE=%lu\n", BATCH_SIZE);
    printf("filesize=%ld\n", filesize);

    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);
        perror("Error opening file");
        return 0;
    }

    buffer = (MemoRecord *)malloc(BATCH_SIZE * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        double start_time_verify = omp_get_wtime();
        double end_time_verify = omp_get_wtime();
        bucket_not_full = false;
        uint64_t distance = 0;
        uint64_t leading_match = 0;
        // uint64_t HASH_SPACE = 1 << (K+1);
        // uint64_t M = UINT64_MAX;
        // uint64_t expected_distance = M / (HASH_SPACE + 1);
        uint64_t expected_distance = 1ULL << (64 - K);
        // uint64_t min_distance = UINT64_MAX;
        // uint64_t max_distance = 0;
        // uint64_t total_distance = 0;
        uint64_t count = 0;
        uint64_t hash_pass_count = 0;

        for (size_t i = 0; i < records_read; ++i)
        {
            ++total_records;
            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                uint8_t hash_output[HASH_SIZE];

                // Compute Blake3 hash of the nonce
                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, buffer[i].nonce, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);

                if (i >= 1)
                {
                    // Compare the first PREFIX_SIZE bytes of the current hash to the hash from two records back
                    if (memcmp(hash_output, prev_hash, HASH_SIZE) >= 0)
                    {
                        ++count_condition_met;
                    }
                    else
                    {
                        ++count_condition_not_met;
                        if (DEBUG)
                        {
                            printf("Condition not met at record %zu:\n", total_records);
                            printf("Previous nonce: ");
                            for (size_t n = 0; n < NONCE_SIZE; ++n)
                                printf("%02X", prev_hash[n]);
                            printf("\n");
                            printf("Previous hash prefix: ");
                            for (size_t n = 0; n < HASH_SIZE; ++n)
                                printf("%02X", prev_hash[n]);
                            printf("\n");
                            printf("Current nonce: ");
                            for (size_t n = 0; n < NONCE_SIZE; ++n)
                                printf("%02X", buffer[i].nonce[n]);
                            printf("\n");
                            printf("Current hash prefix: ");
                            for (size_t n = 0; n < HASH_SIZE; ++n)
                                printf("%02X", hash_output[n]);
                            printf("\n");
                        }
                    }
                }

                if (i >= 2)
                {
                    // Compute distance against the two-back hash
                    distance = compute_hash_distance(prev_prev_hash, hash_output, HASH_SIZE);
                    // distance = compute_hash_distance(prev_hash, hash_output, HASH_SIZE);
                    leading_match = compute_hash_match_leading(prev_prev_hash, hash_output, HASH_SIZE);
                    // leading_match = compute_hash_match_leading(prev_hash, hash_output, HASH_SIZE);
                    if (total_records % (1024 * 1024) == 0)
                    {
                        // distance = compute_hash_distance_debug(prev_prev_hash, hash_output, HASH_SIZE);

                        // distance = compute_hash_distance_debug(prev_hash, hash_output, HASH_SIZE);
                        printf("compute_hash_distance()): distance=%lu expected_distance=%lu match=%s leading_match=%lu\n",
                               distance, expected_distance, (distance <= expected_distance) ? "true" : "false", leading_match);
                    }

                    // if (distance <= expected_distance && leading_match >= K) {
                    if (distance <= expected_distance)
                    {
                        hash_pass_count++;
                    }
                    count++;
                }
                // Shift previous hashes: the immediate previous becomes two-back,
                // then update immediate previous with current values.
                if (i >= 1)
                {
                    memcpy(prev_prev_hash, prev_hash, HASH_SIZE);
                    memcpy(prev_prev_nonce, prev_nonce, NONCE_SIZE);
                }

                memcpy(prev_hash, hash_output, HASH_SIZE);
                memcpy(prev_nonce, buffer[i].nonce, NONCE_SIZE);
            }
            else
            {
                ++zero_nonce_count;
                bucket_not_full = true;
            }
        }
        if (bucket_not_full == false)
            full_buckets++;
        end_time_verify = omp_get_wtime();

        hash_pass_count_global += hash_pass_count;
        if (total_records % (1024 * 1024) == 0)
        {
            printf("Hashed passed: %lu out of %lu (%.2f%%)\n",
                   hash_pass_count_global, total_records, hash_pass_count_global * 100.0 / total_records);
        }
        double elapsed_time_verify = end_time_verify - start_time_verify;
        double elapsed_time = omp_get_wtime() - start_time;
        double throughput = (BATCH_SIZE * sizeof(MemoRecord) / elapsed_time_verify) / (1024 * 1024);
        if (total_records % (1024 * 1024) == 0)
            printf("[%.2f] Verify %.2f%%: %.2f MB/s\n", elapsed_time,
                   total_records * sizeof(MemoRecord) * 100.0 / filesize, throughput);
    }

    if (ferror(file))
    {
        perror("Error reading file");
    }
    fclose(file);
    free(buffer);

    printf("sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu storage_efficiency=%.2f bucket_efficiency=%.2f\n",
           count_condition_met, count_condition_not_met, zero_nonce_count, total_records, full_buckets,
           count_condition_met * 100.0 / total_records, full_buckets * 100.0 / (num_buckets));

    return count_condition_met;
}

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

void generate_table2(MemoRecord *sorted_nonces, size_t num_records_in_bucket)
{
    // bucket_not_full = false;
    // uint64_t distance = 0;
    // uint64_t leading_match = 0;
    uint64_t expected_distance = 1ULL << (64 - K);
    // uint64_t min_distance = UINT64_MAX;
    // uint64_t max_distance = 0;
    // uint64_t total_distance = 0;
    // uint64_t count = 0;
    uint64_t hash_pass_count = 0;

    for (size_t i = 0; i < num_records_in_bucket; ++i)
    {
        if (is_nonce_nonzero(sorted_nonces[i].nonce, NONCE_SIZE))
        {
            // Compute Blake3 hash for record i
            uint8_t hash_i[HASH_SIZE];
            blake3_hasher hasher;
            blake3_hasher_init(&hasher);
            blake3_hasher_update(&hasher, sorted_nonces[i].nonce, NONCE_SIZE);
            blake3_hasher_finalize(&hasher, hash_i, HASH_SIZE);

            // Compare hash_i with all subsequent non-zero nonce records
            // could change the upper bound here to be b+2 to span multiple buckets
            for (size_t j = i + 1; j < num_records_in_bucket; ++j)
            {
                // Skip records with zero nonce
                if (!is_nonce_nonzero(sorted_nonces[j].nonce, NONCE_SIZE))
                {
                    continue;
                }

                // Compute Blake3 hash for record j
                uint8_t hash_j[HASH_SIZE];
                // blake3_hasher hasher_j;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, sorted_nonces[j].nonce, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_j, HASH_SIZE);

                // Compute the distance between hash_i and hash_j
                uint64_t distance = compute_hash_distance(hash_i, hash_j, HASH_SIZE);

                // Because data is sorted, break out of the inner loop once the distance exceeds expected_distance
                if (distance > expected_distance)
                {
                    break;
                }

                // The distance is within the expected threshold; count it.
                hash_pass_count++;

                MemoRecord2 record;
                uint8_t hash_table2[HASH_SIZE];
                generate2Blake3(hash_table2, &record, (unsigned long long)sorted_nonces[i].nonce, (unsigned long long)sorted_nonces[j].nonce);

                // uint8_t hash_table2[HASH_SIZE];
                // blake3_hasher hasher_j;
                // blake3_hasher_init(&hasher);
                // blake3_hasher_update(&hasher, sorted_nonces[i].nonce, NONCE_SIZE);
                // blake3_hasher_update(&hasher, sorted_nonces[j].nonce, NONCE_SIZE);
                // lake3_hasher_finalize(&hasher, hash_table2, HASH_SIZE);

                if (MEMORY_WRITE)
                {
                    off_t bucketIndex = getBucketIndex(hash_table2, PREFIX_SIZE);
                    insert_record2(buckets2, &record, bucketIndex);
                }
                // buckets2_count[bucketIndex]++;
                // printf("bucketIndex=%ld\n",bucketIndex);

                // print_table2_entry(sorted_nonces[i].nonce, sorted_nonces[j].nonce, hash_i, hash_j, HASH_SIZE);

                // Optional: Print debug information every 1,048,576 records processed.
                // if (total_records % (1024 * 1024) == 0)
                //{
                //	printf("compute_hash_distance(): distance=%llu, expected_distance=%llu, hash_pass_count=%llu\n",
                //			distance, expected_distance,hash_pass_count);
                //}
            }
            // Count this nonzero nonce as processed.
            //	count++;
        }
        // else
        //{
        //	++zero_nonce_count;
        //	bucket_not_full = true;
        // }
    }
}

/**
 * Converts a given string to an array of uint8_t.
 *
 * @param SEARCH_STRING The input string to convert.
 * @return A pointer to the array of uint8_t, or NULL if allocation fails.
 */
uint8_t *convert_string_to_uint8_array(const char *SEARCH_STRING)
{
    if (SEARCH_STRING == NULL)
    {
        return NULL;
    }

    size_t length = strlen(SEARCH_STRING);
    uint8_t *array = (uint8_t *)malloc(length * sizeof(uint8_t));
    if (array == NULL)
    {
        // Memory allocation failed
        return NULL;
    }

    for (size_t i = 0; i < length; ++i)
    {
        array[i] = (uint8_t)SEARCH_STRING[i];
    }

    return array;
}

uint8_t *hexStringToByteArray(const char *hexString)
{

    size_t hexLen = strlen(hexString);
    uint8_t *byteArray = (uint8_t *)malloc(hexLen * sizeof(uint8_t));
    // size_t hexLen = strlen(hexString);
    if (hexLen % 2 != 0)
    {
        return NULL; // Error: Invalid hexadecimal string length
    }

    size_t byteLen = hexLen / 2;
    size_t byteArraySize = byteLen;
    if (byteLen > byteArraySize)
    {
        return NULL; // Error: Byte array too small
    }

    for (size_t i = 0; i < byteLen; ++i)
    {
        if (sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]) != 1)
        {
            return NULL; // Error: Failed to parse hexadecimal string
        }
    }

    return byteArray;
}

MemoRecord2 *search_memo_record(FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH, unsigned long long num_records_in_bucket_search, MemoRecord2 *buffer)
{
    const int HASH_SIZE_SEARCH = 8;
    // unsigned long long fRecord = -1;
    size_t records_read;
    // Define the offset you want to seek to
    MemoRecord2 *foundRecord = NULL;

    long offset = bucketIndex * num_records_in_bucket_search * sizeof(MemoRecord2); // For example, seek to byte 1024 from the beginning
    if (DEBUG)
        printf("SEARCH: seek to %zu offset\n", offset);

    // Seek to the specified offset
    if (fseek(file, offset, SEEK_SET) != 0)
    {
        perror("Error seeking in file");
        fclose(file);
        return NULL;
    }

    records_read = fread(buffer, sizeof(MemoRecord2), num_records_in_bucket_search, file);
    if (records_read > 0)
    {
        int found = 0; // Shared flag to indicate termination

#pragma omp parallel shared(found)
        {
#pragma omp for
            for (size_t i = 0; i < records_read; ++i)
            {
                //++total_records;

                // Check for cancellation
#pragma omp cancellation point for
                if (!found && is_nonce_nonzero(buffer[i].nonce1, NONCE_SIZE) && is_nonce_nonzero(buffer[i].nonce2, NONCE_SIZE))
                {
                    uint8_t hash_output[HASH_SIZE_SEARCH];

                    // Compute Blake3 hash of the nonce
                    blake3_hasher hasher;
                    blake3_hasher_init(&hasher);
                    blake3_hasher_update(&hasher, buffer[i].nonce1, NONCE_SIZE);
                    blake3_hasher_update(&hasher, buffer[i].nonce2, NONCE_SIZE);
                    blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE_SEARCH);

                    // print bucket contents
                    if (DEBUG)
                    {

                        printf("bucket[");

                        // printf("Search hash prefix (UINT8): ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", SEARCH_UINT8[n]);
                        printf("][%zu] = ", i);

                        // printf("Current nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce1[n]);
                        printf(" & ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce2[n]);
                        printf(" => ");
                        // printf("Current hash prefix: ");
                        for (size_t n = 0; n < HASH_SIZE_SEARCH; ++n)
                            printf("%02X", hash_output[n]);
                        printf("\n");
                    }

                    // Compare the first PREFIX_SIZE bytes of the current hash to the previous hash prefix
                    if (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0)
                    {
                        // Current hash's first PREFIX_SIZE bytes are equal to or greater than previous
                        //++count_condition_met;
                        // fRecord = buffer[i];
                        // foundRecord = true;
                        // return byteArrayToLongLong(buffer[i].nonce,NONCE_SIZE);
                        // Signal cancellation

                        foundRecord = &buffer[i];
#pragma omp atomic write

                        // if (!DEBUG)
                        //{

                        found = 1;

#pragma omp cancel for
                        //}
                    }
                    else
                    {
                        //++count_condition_not_met;

                        /*
                                            if (!DEBUG)
                                            {
                                            // Print previous hash and nonce, and current hash and nonce
                                            //printf("Condition not met at record %zu:\n", total_records);
                                            //printf("Search string %s\n",SEARCH_STRING);

                                            printf("Search hash prefix (UINT8): ");
                                            for (size_t n = 0; n < PREFIX_SIZE; ++n)
                                                printf("%02X", SEARCH_UINT8[n]);
                                            printf("\n");

                                            printf("Current nonce: ");
                                            for (size_t n = 0; n < NONCE_SIZE; ++n)
                                                printf("%02X", buffer[i].nonce[n]);
                                            printf("\n");
                                            printf("Current hash prefix: ");
                                            for (size_t n = 0; n < HASH_SIZE_SEARCH; ++n)
                                                printf("%02X", hash_output[n]);
                                            printf("\n");
                                            }
                                            */
                    }
                }
            }
        }
    }
    else
    {
        printf("error reading from file..\n");
    }
    return foundRecord;
}

// not sure if the search of more than PREFIX_LENGTH works
void search_memo_records(const char *filename, const char *SEARCH_STRING)
{

    uint8_t *SEARCH_UINT8 = hexStringToByteArray(SEARCH_STRING);
    size_t SEARCH_LENGTH = strlen(SEARCH_STRING) / 2;
    off_t bucketIndex = getBucketIndex(SEARCH_UINT8, PREFIX_SIZE);
    // uint8_t *SEARCH_UINT8 = convert_string_to_uint8_array(SEARCH_STRING);
    // num_records_in_bucket
    MemoRecord2 *buffer = NULL;
    // size_t total_records = 0;
    // size_t zero_nonce_count = 0;

    FILE *file = NULL;
    // uint8_t prev_hash[PREFIX_SIZE] = {0}; // Initialize previous hash prefix to zero
    // uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    // size_t count_condition_met = 0;       // Counter for records meeting the condition
    // size_t count_condition_not_met = 0;
    bool foundRecord = false;
    // MemoRecord fRecord;
    MemoRecord2 *fRecord = NULL;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
    unsigned long long num_records_in_bucket_search = filesize / num_buckets_search / sizeof(MemoRecord2);
    if (!BENCHMARK)
    {
        printf("SEARCH: filename=%s\n", filename);
        printf("SEARCH: filesize=%zu\n", filesize);
        printf("SEARCH: num_buckets=%llu\n", num_buckets_search);
        printf("SEARCH: num_records_in_bucket=%llu\n", num_records_in_bucket_search);
        printf("SEARCH: SEARCH_STRING=%s\n", SEARCH_STRING);
    }

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord2 *)malloc(num_records_in_bucket_search * sizeof(MemoRecord2));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    fRecord = search_memo_record(file, bucketIndex, SEARCH_UINT8, SEARCH_LENGTH, num_records_in_bucket_search, buffer);
    if (fRecord != NULL)
        foundRecord = true;
    else
        foundRecord = false;

    double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    if (foundRecord == true)
        printf("NONCE found (%s, %s) for HASH prefix %s\n", fRecord->nonce1, fRecord->nonce2, SEARCH_STRING);
    else
        printf("no NONCE found for HASH prefix %s\n", SEARCH_STRING);
    printf("search time %.2f ms\n", elapsed_time);

    // return NULL;
}

// not sure if the search of more than PREFIX_LENGTH works
void search_memo_records_batch(const char *filename, int num_lookups, int search_size)
{

    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));

    // uint8_t *SEARCH_UINT8 = hexStringToByteArray("000000");
    size_t SEARCH_LENGTH = search_size;
    // uint8_t *SEARCH_UINT8 = convert_string_to_uint8_array(SEARCH_STRING);
    // num_records_in_bucket
    MemoRecord2 *buffer = NULL;
    // size_t total_records = 0;
    // size_t zero_nonce_count = 0;

    FILE *file = NULL;
    // uint8_t prev_hash[PREFIX_SIZE] = {0}; // Initialize previous hash prefix to zero
    // uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    // size_t count_condition_met = 0;       // Counter for records meeting the condition
    // size_t count_condition_not_met = 0;
    int foundRecords = 0;
    int notFoundRecords = 0;
    MemoRecord2 *fRecord = NULL;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
    unsigned long long num_records_in_bucket_search = filesize / num_buckets_search / sizeof(MemoRecord2);
    if (!BENCHMARK)
    {
        printf("SEARCH: filename=%s\n", filename);
        printf("SEARCH: filesize=%zu\n", filesize);
        printf("SEARCH: num_buckets=%llu\n", num_buckets_search);
        printf("SEARCH: num_records_in_bucket=%llu\n", num_records_in_bucket_search);
    }
    // printf("SEARCH: SEARCH_STRING=%s\n",SEARCH_STRING);

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord2 *)malloc(num_records_in_bucket_search * sizeof(MemoRecord2));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    uint8_t SEARCH_UINT8[search_size];

    for (int i = 0; i < num_lookups; i++)
    {

        for (size_t i = 0; i < (size_t)search_size; ++i)
        {
            SEARCH_UINT8[i] = rand() % 256;
        }

        fRecord = search_memo_record(file, getBucketIndex(SEARCH_UINT8, PREFIX_SIZE), SEARCH_UINT8, SEARCH_LENGTH, num_records_in_bucket_search, buffer);

        if (fRecord != NULL)
            foundRecords++;
        else
            notFoundRecords++;
    }

    double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    // if (foundRecord == true)
    //	printf("NONCE found (%zu) for HASH prefix %s\n",fRecord,SEARCH_STRING);
    // else
    //	printf("no NONCE found for HASH prefix %s\n",SEARCH_STRING);
    if (!BENCHMARK)
        printf("searched for %d lookups of %d bytes long, found %d, not found %d in %.2f seconds, %.2f ms per lookup\n", num_lookups, search_size, foundRecords, notFoundRecords, elapsed_time / 1000.0, elapsed_time / num_lookups);
    else
        printf("%s %d %zu %llu %llu %d %d %d %d %.2f %.2f\n", filename, NUM_THREADS, filesize, num_buckets_search, num_records_in_bucket_search, num_lookups, search_size, foundRecords, notFoundRecords, elapsed_time / 1000.0, elapsed_time / num_lookups);
    // return NULL;
}

uint64_t largest_power_of_two_less_than(uint64_t number)
{
    if (number == 0)
    {
        return 0;
    }

    // Decrement number to handle cases where number is already a power of 2
    number--;

    // Set all bits to the right of the most significant bit
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number |= number >> 32; // Only needed for 64-bit integers

    // The most significant bit is now set; shift right to get the largest power of 2 less than the original number
    return (number + 1) >> 1;
}

int rename_file(const char *old_name, const char *new_name)
{
    // Attempt to rename the file
    if (rename(old_name, new_name) != 0)
    {
        // If rename fails, perror prints a descriptive error message
        perror("Error renaming file");
        return -1;
    }

    // Success
    return 0;
}

void remove_file(const char *fileName)
{
    // Attempt to remove the file
    if (remove(fileName) == 0)
    {
        if (DEBUG)
            printf("File '%s' removed successfully.\n", fileName);
    }
    else
    {
        perror("Error removing file");
    }
}

int move_file_overwrite(const char *source_path, const char *destination_path)
{
    if (DEBUG)
        printf("move_file_overwrite()...\n");
    // Attempt to rename the file
    if (rename(source_path, destination_path) == 0)
    {
        if (!BENCHMARK)
            printf("rename success!\n");
        return 0;
    }

    // If rename failed, check if it's due to cross-device move
    if (errno != EXDEV)
    {
        perror("Error renaming file");
        return -1;
    }

    // Proceed to copy and delete since it's a cross-filesystem move

    // Remove the destination file if it exists to allow overwriting
    if (remove(destination_path) != 0 && errno != ENOENT)
    {
        perror("Error removing existing destination file");
        return -1;
    }

    // Continue with copying as in the original move_file function
    FILE *source = fopen(source_path, "rb");
    if (source == NULL)
    {
        perror("Error opening source file for reading");
        return -1;
    }

    FILE *destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        perror("Error opening destination file for writing");
        fclose(source);
        return -1;
    }

    if (!BENCHMARK)
        printf("deep copy started...\n");
    size_t buffer_size = 1024 * 1024 * 8; // 1 MB
    uint8_t *buffer = (uint8_t *)calloc(buffer_size, 1);

    if (buffer == NULL)
    {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    // char buffer[1024*1024];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0)
    {
        size_t bytes_written = fwrite(buffer, 1, bytes, destination);
        if (bytes_written != bytes)
        {
            perror("Error writing to destination file");
            fclose(source);
            fclose(destination);
            return -1;
        }
    }

    if (ferror(source))
    {
        perror("Error reading from source file");
        fclose(source);
        fclose(destination);
        return -1;
    }

    if (fflush(source) != 0)
    {
        perror("Failed to flush buffer");
        fclose(source);
        return EXIT_FAILURE;
    }

    if (fsync(fileno(source)) != 0)
    {
        perror("Failed to fsync buffer");
        fclose(source);
        return EXIT_FAILURE;
    }

    fclose(source);

    if (fflush(destination) != 0)
    {
        perror("Failed to flush buffer");
        fclose(destination);
        return EXIT_FAILURE;
    }

    if (fsync(fileno(destination)) != 0)
    {
        perror("Failed to fsync buffer");
        fclose(destination);
        return EXIT_FAILURE;
    }

    fclose(destination);

    if (remove(source_path) != 0)
    {
        perror("Error deleting source file after moving");
        return -1;
    }

    if (!BENCHMARK)
        printf("deep copy finished!\n");
    if (DEBUG)
        printf("move_file_overwrite() finished!\n");
    return 0; // Success
}

long long fib(int n);
long long fib_seq(int n);
// static long long result;

void handler(int sig)
{
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// could modify for different NONCE_SIZE
unsigned long long random_ull(void)
{
    // We know rand() gives at least 15 bits.
    // We'll combine five 15-bit segments to produce a 64-bit number.
    // This gives us at least 75 bits of randomness, but we only use 64.
    // The top bits are simply truncated by shifting.

    unsigned long long r = 0ULL;
    r |= (unsigned long long)(rand() & 0x7FFFULL);         // 15 bits
    r |= ((unsigned long long)(rand() & 0x7FFFULL)) << 15; // +15 bits = 30 bits
    r |= ((unsigned long long)(rand() & 0x7FFFULL)) << 30; // +15 bits = 45 bits
                                                           //    r |= ((unsigned long long)(rand() & 0x7FFFULL)) << 45;     // +15 bits = 60 bits

    // We only need 4 more bits to reach 64 bits, but we'll just take another
    // full 15 bits and let the upper bits be masked by the width of unsigned long long.
    //    r |= ((unsigned long long)(rand() & 0x7FFFULL)) << 60;     // now we have at least 64 bits

    return r;
}

// Function to generate a random number using rand_r() with time as the seed
unsigned int random_with_time_seed()
{
    unsigned int seed = (unsigned int)time(NULL); // Initialize the seed with the current time
    return (unsigned int)rand_r(&seed);           // Generate and return a random number
}

// Function to generate a random number using rand_r() with clock_gettime() as the seed
// unsigned int random_with_high_res_seed()
// {
//     struct timespec ts;
//     clock_gettime(CLOCK_REALTIME, &ts); // Get current time with nanosecond resolution

//     // Use nanoseconds part of the timestamp as the seed
//     unsigned int seed = (unsigned int)(ts.tv_nsec);
//     return (unsigned int)rand_r(&seed);
// }

// using namespace std;

long long fib(int n)
{
    // printf("[tid=%d]fib(%d), run fib(%d) + fib(%d).\n", omp_get_thread_num(), n, n-1, n-2);
    long long x, y;
    if (n < 2)
    {

        unsigned long long nonce = random_ull();
        // unsigned int nonce = random_with_time_seed();
        // unsigned long long nonce = (unsigned long long)random_with_high_res_seed();
        MemoRecord record;
        uint8_t record_hash[HASH_SIZE];

        // unsigned long long batch_end = i + BATCH_SIZE;
        // if (batch_end > end_idx) {
        //     batch_end = end_idx;
        // }

        // for (unsigned long long j = i; j < batch_end; j++)
        //{
        generateBlake3(record_hash, &record, nonce);
        if (MEMORY_WRITE)
        {
            off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
            insert_record(buckets, &record, bucketIndex);
        }
        //}

        return n;
    }
    // if (n == 10) return 55;
    // if(n == 9) return 34;

#pragma omp task untied shared(x) firstprivate(n)
    x = fib(n - 1);
#pragma omp task untied shared(y) firstprivate(n)
    y = fib(n - 2);

#pragma omp taskwait
    return x + y;
}

long long fib_seq(int n)
{
    int x, y;
    if (n < 2)
        return n;

    x = fib_seq(n - 1);
    y = fib_seq(n - 2);

    return x + y;
}

// Recursive function to generate numbers using divide and conquer
void generateHashes(unsigned long long start, unsigned long long end)
{
    // if (start > end) {
    //     return; // Base case: no numbers to print
    // }
    // if (start == end) {
    if (end - start < BATCH_SIZE)
    {
        // printf("%d\n", start); // Print a single number

        MemoRecord record;
        uint8_t record_hash[HASH_SIZE];

        // unsigned long long batch_end = i + BATCH_SIZE;
        // if (batch_end > end_idx) {
        //     batch_end = end_idx;
        // }

        for (unsigned long long j = start; j <= end; j++)
        {
            generateBlake3(record_hash, &record, j);
            if (MEMORY_WRITE)
            {
                off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                insert_record(buckets, &record, bucketIndex);
            }
        }

        return;
    }

    unsigned long long mid = (start + end) / 2;

// Recursive calls for the left and right halves
#pragma omp task shared(start, mid) // Task for the left half
    generateHashes(start, mid);     // Generate numbers in the left half

#pragma omp task shared(mid, end) // Task for the right half
    generateHashes(mid + 1, end); // Generate numbers in the right half

#pragma omp taskwait // Wait for both tasks to complete
}

int main(int argc, char *argv[])
{
    // Default values
    const char *approach = "for"; // Default approach
    int num_threads = 0;          // 0 means OpenMP chooses
    int num_threads_io = 0;
    unsigned long long num_iterations = 1ULL << K; // 2^K iterations
    unsigned long long num_hashes = num_iterations;
    unsigned long long MEMORY_SIZE_MB = 1;
    // unsigned long long MEMORY_SIZE_bytes_original = 0;
    char *FILENAME = NULL;        // Default output file name
    char *FILENAME_FINAL = NULL;  // Default output file name
    char *FILENAME_TABLE2 = NULL; // Default output file name
    char *SEARCH_STRING = NULL;   // Default output file name

    // Define long options
    static struct option long_options[] = {
        {"approach", required_argument, 0, 'a'},
        {"threads", required_argument, 0, 't'},
        {"threads_io", required_argument, 0, 'i'},
        {"exponent", required_argument, 0, 'K'},
        {"memory", required_argument, 0, 'm'},
        {"file", required_argument, 0, 'f'},
        {"file_final", required_argument, 0, 'g'},
        {"file_table2", required_argument, 0, 'j'},
        {"batch-size", required_argument, 0, 'b'},
        {"memory_write", required_argument, 0, 'w'},
        {"circular_array", required_argument, 0, 'c'},
        {"verify", required_argument, 0, 'v'},
        {"search", required_argument, 0, 's'},
        {"prefix_search_size", required_argument, 0, 'p'},
        {"benchmark", required_argument, 0, 'x'},
        {"full_buckets", required_argument, 0, 'y'},
        {"debug", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    // add option x for benchmark

    int opt;
    int option_index = 0;

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:f:g:j:b:w:c:v:s:p:x:y:d:h", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'a':
            if (strcmp(optarg, "xtask") == 0 || strcmp(optarg, "task") == 0 || strcmp(optarg, "for") == 0 || strcmp(optarg, "tbb") == 0)
            {
                approach = optarg;
            }
            else
            {
                fprintf(stderr, "Invalid approach: %s\n", optarg);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            num_threads = atoi(optarg);
            if (num_threads <= 0)
            {
                fprintf(stderr, "Number of threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            num_threads_io = atoi(optarg);
            if (num_threads_io <= 0)
            {
                fprintf(stderr, "Number of I/O threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'K':
            K = atoi(optarg);
            if (K < 24 || K > 40)
            { // Limiting K to avoid overflow
                fprintf(stderr, "Exponent K must be between 24 and 40.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            num_iterations = 1ULL << K; // Compute 2^K
            break;
        case 'm':
            MEMORY_SIZE_MB = atoi(optarg);
            // MEMORY_SIZE_bytes_original = MEMORY_SIZE_MB * 1024 * 1024;
            if (MEMORY_SIZE_MB < 64)
            {
                fprintf(stderr, "Memory size must be at least 64 MB.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'f':
            FILENAME = optarg;
            writeData = true;
            break;
        case 'g':
            FILENAME_FINAL = optarg;
            writeDataFinal = true;
            break;
        case 'j':
            FILENAME_TABLE2 = optarg;
            writeDataTable2 = true;
            break;
        case 'b':
            BATCH_SIZE = atoi(optarg);
            if (BATCH_SIZE < 1)
            {
                fprintf(stderr, "BATCH_SIZE must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'w':
            if (strcmp(optarg, "true") == 0)
            {
                MEMORY_WRITE = true;
            }
            else
            {
                MEMORY_WRITE = false;
            }
            break;
        case 'c':
            if (strcmp(optarg, "true") == 0)
            {
                CIRCULAR_ARRAY = true;
            }
            else
            {
                CIRCULAR_ARRAY = false;
            }
            break;
        case 'v':
            if (strcmp(optarg, "true") == 0)
            {
                VERIFY = true;
            }
            else
            {
                VERIFY = false;
            }
            break;
        case 's':
            SEARCH_STRING = optarg;
            SEARCH = true;
            HASHGEN = false;
            break;
        case 'p':
            SEARCH_BATCH = true;
            SEARCH = true;
            HASHGEN = false;
            PREFIX_SEARCH_SIZE = atoi(optarg);
            if (PREFIX_SEARCH_SIZE < 1)
            {
                fprintf(stderr, "PREFIX_SEARCH_SIZE must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'x':
            if (strcmp(optarg, "true") == 0)
            {
                BENCHMARK = true;
            }
            else
            {
                BENCHMARK = false;
            }
            break;
        case 'y':
            if (strcmp(optarg, "true") == 0)
            {
                FULL_BUCKETS = true;
            }
            else
            {
                FULL_BUCKETS = false;
            }
            break;
        case 'd':
            if (strcmp(optarg, "true") == 0)
            {
                DEBUG = true;
            }
            else
            {
                DEBUG = false;
            }
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    NUM_THREADS = num_threads;
    // Set the number of threads if specified
    if (num_threads > 0)
    {
        omp_set_num_threads(num_threads);
    }

    if (num_threads_io == 0)
    {
        num_threads_io = 1;
    }

    // Display selected configurations
    if (!BENCHMARK)
    {
        if (!SEARCH)
        {
            printf("Selected Approach           : %s\n", approach);
            printf("Number of Threads           : %d\n", num_threads > 0 ? num_threads : omp_get_max_threads());
            printf("Number of Threads I/O       : %d\n", num_threads_io > 0 ? num_threads_io : omp_get_max_threads());
            printf("Exponent K                  : %d\n", K);
        }
    }

    unsigned long long file_size_bytes = num_iterations * NONCE_SIZE;
    double file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);
    unsigned long long MEMORY_SIZE_bytes = 0;

    // if (MEMORY_SIZE_MB / 1024.0 > file_size_gb) {
    if (MEMORY_SIZE_MB * 1024 * 1024 > file_size_bytes)
    {
        MEMORY_SIZE_MB = (unsigned long long)(file_size_bytes / (1024 * 1024));
        MEMORY_SIZE_bytes = file_size_bytes;
    }
    else
        MEMORY_SIZE_bytes = MEMORY_SIZE_MB * 1024 * 1024;

    // printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
    // printf("Memory Size (bytes)            : %llu\n", MEMORY_SIZE_bytes);

    rounds = ceil(file_size_bytes / MEMORY_SIZE_bytes);
    MEMORY_SIZE_bytes = file_size_bytes / rounds;
    num_hashes = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    MEMORY_SIZE_bytes = num_hashes * NONCE_SIZE;
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);

    // MEMORY_SIZE_bytes

    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));

    num_hashes = MEMORY_SIZE_bytes / NONCE_SIZE;

    num_buckets = 1ULL << (PREFIX_SIZE * 8);

    num_records_in_bucket = num_hashes / num_buckets;

    MEMORY_SIZE_bytes = num_buckets * num_records_in_bucket * sizeof(MemoRecord);
    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);
    num_hashes = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    num_iterations = num_hashes * rounds;

    if (!BENCHMARK)
    {

        if (SEARCH)
        {
            printf("SEARCH                      : true\n");
            // printf("SEARCH_STRING               : %s\n",SEARCH_STRING);
        }
        else
        {
            // printf("SEARCH                      : false\n");

            printf("File Size (GB)              : %.2f\n", file_size_gb);
            printf("File Size (bytes)           : %llu\n", file_size_bytes);

            printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
            printf("Memory Size (bytes)         : %llu\n", MEMORY_SIZE_bytes);

            printf("Number of Hashes (RAM)      : %llu\n", num_hashes);

            printf("Number of Hashes (Disk)     : %llu\n", num_iterations);
            printf("Size of MemoRecord          : %lu\n", sizeof(MemoRecord));
            printf("Rounds                      : %llu\n", rounds);

            printf("Number of Buckets           : %llu\n", num_buckets);
            printf("Number of Records in Bucket : %llu\n", num_records_in_bucket);

            printf("BATCH_SIZE                  : %zu\n", BATCH_SIZE);

            if (HASHGEN)
                printf("HASHGEN                     : true\n");
            else
                printf("HASHGEN                     : false\n");

            if (MEMORY_WRITE)
                printf("MEMORY_WRITE                : true\n");
            else
                printf("MEMORY_WRITE                : false\n");

            if (CIRCULAR_ARRAY)
                printf("CIRCULAR_ARRAY              : true\n");
            else
                printf("CIRCULAR_ARRAY              : false\n");

            if (FULL_BUCKETS)
                printf("FULL_BUCKETS                : true\n");
            else
                printf("FULL_BUCKETS                : false\n");

            if (writeData)
            {
                printf("Temporary File              : %s\n", FILENAME);
            }
            if (writeDataFinal)
            {
                printf("Output File Table 1         : %s\n", FILENAME_FINAL);
            }
            if (writeDataTable2)
            {
                printf("Output File Table 2         : %s\n", FILENAME_TABLE2);
            }
        }
    }

    if (HASHGEN)
    {
        unsigned long long record_counts = 0;
        if (!BENCHMARK)
            printf("HASHGEN                     : true\n");

        // Open the file for writing in binary mode
        FILE *fd = NULL;
        if (writeData)
        {
            fd = fopen(FILENAME, "wb+");
            if (fd == NULL)
            {
                printf("Error opening file %s (#4)\n", FILENAME);

                perror("Error opening file");
                return EXIT_FAILURE;
            }
        }

        // Start walltime measurement
        double start_time = omp_get_wtime();

        // Allocate memory for the array of Buckets
        buckets = (Bucket *)calloc(num_buckets, sizeof(Bucket));
        if (buckets == NULL)
        {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate memory for each bucket's records
        for (unsigned long long i = 0; i < num_buckets; i++)
        {
            buckets[i].records = (MemoRecord *)calloc(num_records_in_bucket, sizeof(MemoRecord));
            if (buckets[i].records == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
                exit(EXIT_FAILURE);
            }
        }

        // Allocate memory for the array of Buckets
        buckets2 = (Bucket2 *)calloc(num_buckets, sizeof(Bucket2));
        if (buckets2 == NULL)
        {
            fprintf(stderr, "Error: Unable to allocate memory for buckets2.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate memory for each bucket's records
        for (unsigned long long i = 0; i < num_buckets; i++)
        {
            buckets2[i].records = (MemoRecord2 *)calloc(num_records_in_bucket, sizeof(MemoRecord2));
            if (buckets2[i].records == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
                exit(EXIT_FAILURE);
            }
        }

        double throughput_hash = 0.0;
        double throughput_io = 0.0;

        double start_time_io = 0.0;
        double end_time_io = 0.0;
        double elapsed_time_io = 0.0;
        double elapsed_time_io2 = 0.0;
        double start_time_hash = 0.0;
        double end_time_hash = 0.0;
        double elapsed_time_hash = 0.0;

        double elapsed_time_hash_total = 0.0;
        double elapsed_time_io_total = 0.0;
        double elapsed_time_io2_total = 0.0;

        for (unsigned long long r = 0; r < rounds; r++)
        {
            start_time_hash = omp_get_wtime();

            // Reset bucket counts
            for (unsigned long long i = 0; i < num_buckets; i++)
            {
                buckets[i].count = 0;
                buckets[i].count_waste = 0;
                buckets[i].full = false;
                buckets[i].flush = 0;
            }

            unsigned long long MAX_NUM_HASHES = 1ULL << (NONCE_SIZE * 8);
            // if we want to overgenerate hashes to fill all buckets
            if (FULL_BUCKETS)
                num_hashes = MAX_NUM_HASHES / rounds;
            unsigned long long start_idx = r * num_hashes;
            unsigned long long end_idx = start_idx + num_hashes;
            unsigned long long nonce_max = 0;
            // end_idx = end_idx*2;
            // end_idx = 1ULL << (NONCE_SIZE * 8);
            if (!BENCHMARK)
                printf("MAX_NUM_HASHES=%llu rounds=%llu num_hashes=%llu start_idx = %llu, end_idx = %llu\n", MAX_NUM_HASHES, rounds, num_hashes, start_idx, end_idx);

            // uses recursive task based parallelism
            if (strcmp(approach, "xtask") == 0)
            {

                srand((unsigned)time(NULL)); // Seed the random number generator
#pragma omp parallel
                {
#pragma omp single
                    {
                        // could implement BATCH_SIZE here too
                        generateHashes(start_idx, end_idx);
                    }
                }
            }
            else if (strcmp(approach, "task") == 0)
            {
// Tasking Model Approach
#pragma omp parallel
                {
// #pragma omp single nowait
#pragma omp single
                    {
                        for (unsigned long long i = start_idx; i < end_idx; i += BATCH_SIZE)
                        {
#pragma omp task untied
                            {
                                MemoRecord record;
                                uint8_t record_hash[HASH_SIZE];

                                unsigned long long batch_end = i + BATCH_SIZE;
                                if (batch_end > end_idx)
                                {
                                    batch_end = end_idx;
                                }

                                for (unsigned long long j = i; j < batch_end; j++)
                                {
                                    generateBlake3(record_hash, &record, j);
                                    if (MEMORY_WRITE)
                                    {
                                        off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                                        insert_record(buckets, &record, bucketIndex);
                                    }
                                }
                            }
                        }
                    }
                } // Implicit barrier ensures all tasks are completed before exiting
            }
            else if (strcmp(approach, "for") == 0)
            {
                // Parallel For Loop Approach
                // #pragma omp parallel for schedule(dynamic)
                volatile int cancel_flag = 0; // Shared flag
                full_buckets_global = 0;

#pragma omp parallel for schedule(static) shared(cancel_flag)
                for (unsigned long long i = start_idx; i < end_idx; i += BATCH_SIZE)
                {

                    if (cancel_flag)
                        continue;

                    MemoRecord record;
                    uint8_t record_hash[HASH_SIZE];

                    unsigned long long batch_end = i + BATCH_SIZE;
                    if (batch_end > end_idx)
                    {
                        batch_end = end_idx;
                    }

                    for (unsigned long long j = i; j < batch_end; j++)
                    {
                        generateBlake3(record_hash, &record, j);
                        if (MEMORY_WRITE)
                        {
                            off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                            insert_record(buckets, &record, bucketIndex);
                        }
                    }

                    // Set the flag if the termination condition is met.
                    // if (i > end_idx/2 && full_buckets_global == num_buckets) {
                    if (full_buckets_global >= num_buckets)
                    {
                        cancel_flag = 1;
                        if (i > nonce_max)
                            nonce_max = i;
                        // break;
                    }
                }
            }
#ifndef __cplusplus
            // Your C-specific code here
            else if (strcmp(approach, "tbb") == 0)
            {
                printf("TBB is not supported with C, use C++ compiler instead to build vaultx, exiting...\n");
                exit(1);
            }
#endif

#ifdef __cplusplus
            // Your C++-specific code here

            else if (strcmp(approach, "tbb") == 0)
            {
                // Parallel For Loop Approach
                tbb::parallel_for(
                    tbb::blocked_range<unsigned long long>(start_idx, end_idx, BATCH_SIZE),
                    [&](const tbb::blocked_range<unsigned long long> &batch_range)
                    {
                        // Process each batch in the range
                        for (unsigned long long i = batch_range.begin(); i < batch_range.end(); i += BATCH_SIZE)
                        {
                            unsigned long long batch_end = i + BATCH_SIZE;
                            if (batch_end > end_idx)
                            {
                                batch_end = end_idx;
                            }

                            for (unsigned long long j = i; j < batch_end; ++j)
                            {
                                MemoRecord record;
                                uint8_t record_hash[HASH_SIZE];

                                generateBlake3(record_hash, &record, j);

                                if (MEMORY_WRITE)
                                {
                                    off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                                    insert_record(buckets, &record, bucketIndex);
                                }
                            }
                        }
                    });
            }
#endif

            // after else if

            // End hash computation time measurement
            end_time_hash = omp_get_wtime();
            elapsed_time_hash = end_time_hash - start_time_hash;
            elapsed_time_hash_total += elapsed_time_hash;

            // Write data to disk if required
            if (writeData)
            {
                start_time_io = omp_get_wtime();

                // Seek to the correct position in the file
                off_t offset = r * num_records_in_bucket * num_buckets * NONCE_SIZE;
                if (fseeko(fd, offset, SEEK_SET) < 0)
                {
                    perror("Error seeking in file");
                    fclose(fd);
                    exit(EXIT_FAILURE);
                }

                size_t bytesWritten = 0;
                // Write buckets to disk

                // Set the number of threads if specified
                if (num_threads_io > 0)
                {
                    omp_set_num_threads(num_threads);
                }

                /*
                        #pragma omp parallel for schedule(static)
                        for (unsigned long long i = 0; i < num_buckets; i++) {
                            //update this to build table2
                            bytesWritten += writeBucketToDiskSequential(&buckets[i], fd);
                            //printf("writeBucketToDiskSequential(): %llu bytes\n",bytesWritten);
                        }*/

#pragma omp parallel for schedule(static)
                for (unsigned long long i = 0; i < num_buckets; i++)
                {

                    // printf("num_records_in_bucket=%llu sizeof(MemoRecord)=%d\n",num_records_in_bucket,sizeof(MemoRecord));
                    // need to store this better
                    // MemoRecord *sorted_nonces = sort_bucket_records(buckets[i].records, num_records_in_bucket);
                    sort_bucket_records_inplace(buckets[i].records, num_records_in_bucket);
                    generate_table2(buckets[i].records, num_records_in_bucket);

                    // size_t elementsWritten = fwrite(buckets[i].records, sizeof(MemoRecord), num_records_in_bucket, fd);
                    // size_t elementsWritten = fwrite(sorted_nonces, sizeof(MemoRecord), num_records_in_bucket, fd);
                    // if (elementsWritten != num_records_in_bucket) {
                    //     fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                    //             elementsWritten, num_records_in_bucket);
                    //     fclose(fd);
                    //     exit(EXIT_FAILURE);
                    // }
                    // bytesWritten += elementsWritten*sizeof(MemoRecord);
                }

                // Set the number of threads if specified
                // if (num_threads_io > 0) {
                //     omp_set_num_threads(num_threads_io);
                // }

                // write table1
                // 		#pragma omp parallel for schedule(static)
                /*
                         for (unsigned long long i = 0; i < num_buckets; i++) {

                 //printf("num_records_in_bucket=%llu sizeof(MemoRecord)=%d\n",num_records_in_bucket,sizeof(MemoRecord));

                 //MemoRecord *sorted_nonces = sort_bucket_records(buckets[i].records, num_records_in_bucket);

                 //size_t elementsWritten = fwrite(buckets[i].records, sizeof(MemoRecord), num_records_in_bucket, fd);
                 size_t elementsWritten = fwrite(buckets[i].records, sizeof(MemoRecord), num_records_in_bucket, fd);
                 if (elementsWritten != num_records_in_bucket) {
                     fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                             elementsWritten, num_records_in_bucket);
                     fclose(fd);
                     exit(EXIT_FAILURE);
                 }
                 bytesWritten += elementsWritten*sizeof(MemoRecord);
                         }            */

                // write table2
                // 		#pragma omp parallel for schedule(static)
                for (unsigned long long i = 0; i < num_buckets; i++)
                {
                    size_t elementsWritten = fwrite(buckets2[i].records, sizeof(MemoRecord2), num_records_in_bucket, fd);
                    if (elementsWritten != num_records_in_bucket)
                    {
                        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                                elementsWritten, num_records_in_bucket);
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }
                    bytesWritten += elementsWritten * sizeof(MemoRecord2);
                }

                // printf("writeBucketToDiskSequential(): %llu bytes at offset %llu; num_hashes=%llu\n",bytesWritten,offset,num_hashes);

                // End I/O time measurement
                end_time_io = omp_get_wtime();
                elapsed_time_io = end_time_io - start_time_io;
                elapsed_time_io_total += elapsed_time_io;

                // count how many full buckets , this works for rounds == 1
                // if (rounds == 1)
                //{
                unsigned long long full_buckets = 0;
                // unsigned long long record_counts = 0;
                unsigned long long record_counts_waste = 0;
                for (unsigned long long i = 0; i < num_buckets; i++)
                {
                    if (buckets2[i].count == num_records_in_bucket)
                        full_buckets++;
                    record_counts += buckets2[i].count;
                    record_counts_waste += buckets2[i].count_waste;
                }

                if (!BENCHMARK)
                {
                    printf("record_counts=%llu storage_efficiency=%.2f full_buckets=%llu bucket_efficiency=%.2f nonce_max=%llu record_counts_waste=%llu hash_efficiency=%.2f\n", record_counts, record_counts * 100.0 / (num_buckets * num_records_in_bucket), full_buckets, full_buckets * 100.0 / num_buckets, nonce_max, record_counts_waste, num_buckets * num_records_in_bucket * 100.0 / (record_counts_waste + num_buckets * num_records_in_bucket));
                }

                // printf("%.2f MB/s\n", throughput_io);
            }

            // Calculate throughput (hashes per second)
            throughput_hash = (num_hashes / (elapsed_time_hash + elapsed_time_io)) / (1e6);

            // Calculate I/O throughput
            throughput_io = (num_hashes * NONCE_SIZE * 2) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);

            if (!BENCHMARK)
                printf("[%.2f] HashGen %.2f%%: %.2f MH/s : I/O %.2f MB/s\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds, throughput_hash, throughput_io);
            // end of loop
            // else {
            //    printf("\n");
            //}
        }

        start_time_io = omp_get_wtime();

        // Flush and close the file
        if (writeData)
        {
            if (fflush(fd) != 0)
            {
                perror("Failed to flush buffer");
                fclose(fd);
                return EXIT_FAILURE;
            }
            // fclose(fd);
        }

        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        // should move timing for I/O to after this section of code

        /*
        // Verification if enabled
        if (VERIFY) {
            unsigned long long num_zero = 0;
            for (unsigned long long i = 0; i < num_buckets; i++) {
                for (unsigned long long j = 0; j < buckets[i].count; j++) {
                    if (byteArrayToLongLong(buckets[i].records[j].nonce, NONCE_SIZE) == 0)
                        num_zero++;
                }
            }
            printf("Number of zero nonces: %llu\n", num_zero);
        }*/

        // Free allocated memory
        for (unsigned long long i = 0; i < num_buckets; i++)
        {
            free(buckets[i].records);
            free(buckets2[i].records);
        }
        free(buckets);
        free(buckets2);

        // if (writeData)

        if (writeDataFinal && rounds > 1)
        {
            // Open the file for writing in binary mode
            FILE *fd_dest = NULL;
            if (writeDataFinal)
            {
                fd_dest = fopen(FILENAME_FINAL, "wb+");
                if (fd_dest == NULL)
                {
                    printf("Error opening file %s (#5)\n", FILENAME_FINAL);
                    perror("Error opening file");

                    // perror("Error opening file");
                    return EXIT_FAILURE;
                }
            }

            unsigned long long num_buckets_to_read = ceil((MEMORY_SIZE_bytes / (num_records_in_bucket * rounds * NONCE_SIZE)) / 2);
            if (DEBUG)
                printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
            // need to fix this for 5 byte NONCE_SIZE
            if (num_buckets % num_buckets_to_read != 0)
            {
                uint64_t ratio = num_buckets / num_buckets_to_read;
                uint64_t result = largest_power_of_two_less_than(ratio);
                if (DEBUG)
                    printf("Largest power of 2 less than %lu is %lu\n", ratio, result);
                num_buckets_to_read = num_buckets / result;
                if (DEBUG)
                    printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
                // printf("error, num_buckets_to_read is not a multiple of num_buckets, exiting: num_buckets=%llu num_buckets_to_read=%llu...\n",num_buckets,num_buckets_to_read);
                // return EXIT_FAILURE;
            }

            // Calculate the total number of records to read per batch
            size_t records_per_batch = num_records_in_bucket * num_buckets_to_read;
            // Calculate the size of the buffer needed
            size_t buffer_size = records_per_batch * rounds;
            // Allocate the buffer
            if (DEBUG)
                printf("allocating %lu bytes for buffer\n", buffer_size * sizeof(MemoRecord));
            MemoRecord *buffer = (MemoRecord *)malloc(buffer_size * sizeof(MemoRecord));
            if (buffer == NULL)
            {
                fprintf(stderr, "Error allocating memory for buffer.\n");
                exit(EXIT_FAILURE);
            }

            if (DEBUG)
                printf("allocating %lu bytes for bufferShuffled\n", buffer_size * sizeof(MemoRecord));
            MemoRecord *bufferShuffled = (MemoRecord *)malloc(buffer_size * sizeof(MemoRecord));
            if (bufferShuffled == NULL)
            {
                fprintf(stderr, "Error allocating memory for bufferShuffled.\n");
                exit(EXIT_FAILURE);
            }

            // Set the number of threads if specified
            if (num_threads_io > 0)
            {
                omp_set_num_threads(num_threads_io);
            }

            for (unsigned long long i = 0; i < num_buckets; i = i + num_buckets_to_read)
            {
                double start_time_io2 = omp_get_wtime();

#pragma omp parallel for schedule(static)
                for (unsigned long long r = 0; r < rounds; r++)
                {
                    // off_t offset_src_old = r * num_records_in_bucket * num_buckets * NONCE_SIZE + i*num_records_in_bucket*NONCE_SIZE;
                    //  Calculate the source offset
                    off_t offset_src = ((r * num_buckets + i) * num_records_in_bucket) * sizeof(MemoRecord);
                    // printf("read data: offset_src_old=%llu offset_src=%llu\n",offset_src_old,offset_src);
                    // if (DEBUG) printf("read data: offset_src_old=%llu bytes=%llu\n",offset_src_old,num_records_in_bucket*NONCE_SIZE*num_buckets_to_read);
                    if (DEBUG)
                        printf("read data: offset_src=%lu bytes=%lu\n",
                               offset_src, records_per_batch * sizeof(MemoRecord));

                    if (fseeko(fd, offset_src, SEEK_SET) < 0)
                    {
                        perror("Error seeking in file");
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }

                    // size_t recordsRead = fread(buffer+num_records_in_bucket*num_buckets_to_read*sizeof(MemoRecord)*r, sizeof(MemoRecord), num_records_in_bucket*num_buckets_to_read, fd);
                    //  Correct pointer arithmetic

                    size_t index = r * records_per_batch;
                    if (DEBUG)
                        printf("storing read data at index %lu\n", index);
                    size_t recordsRead = fread(&buffer[index],
                                               sizeof(MemoRecord),
                                               records_per_batch,
                                               fd);
                    // size_t recordsRead = fread(buffer + r * records_per_batch,
                    //                        sizeof(MemoRecord),
                    //                        records_per_batch,
                    //                        fd);
                    if (recordsRead != records_per_batch)
                    {
                        fprintf(stderr, "Error reading file, records read %zu instead of %zu\n",
                                recordsRead, records_per_batch);
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        if (DEBUG)
                            printf("read %zu records from disk...\n", recordsRead);
                    }

                    off_t offset_dest = i * num_records_in_bucket * NONCE_SIZE * rounds;
                    if (DEBUG)
                        printf("write data: offset_dest=%lu bytes=%llu\n", offset_dest, num_records_in_bucket * NONCE_SIZE * rounds * num_buckets_to_read);

                    if (fseeko(fd_dest, offset_dest, SEEK_SET) < 0)
                    {
                        perror("Error seeking in file");
                        fclose(fd_dest);
                        exit(EXIT_FAILURE);
                    }
                    // needs to make sure its ok, fix things....
                    // printf("buffer_size=%llu my_buffer_size=%llu\n",buffer_size,num_records_in_bucket*num_buckets_to_read*rounds);
                }
                // end of for loop rounds

                if (DEBUG)
                    printf("shuffling %llu buckets with %llu bytes each...\n", num_buckets_to_read * rounds, num_records_in_bucket * NONCE_SIZE);
#pragma omp parallel for schedule(static)
                for (unsigned long long s = 0; s < num_buckets_to_read; s++)
                {
                    for (unsigned long long r = 0; r < rounds; r++)
                    {
                        // off_t index_src = s*num_records_in_bucket+r*num_records_in_bucket*rounds;
                        // off_t index_dest = s + r*num_records_in_bucket*rounds;

                        off_t index_src = ((r * num_buckets_to_read + s) * num_records_in_bucket);
                        off_t index_dest = (s * rounds + r) * num_records_in_bucket;

                        // printf("SHUFFLE: index_src=%llu index_dest=%llu\n",index_src,index_dest);
                        // printf("SHUFFLE: s=%llu, r=%llu, index_src=%llu, index_dest=%llu\n", s, r, index_src, index_dest);

                        memcpy(&bufferShuffled[index_dest], &buffer[index_src], num_records_in_bucket * sizeof(MemoRecord));
                    }
                }
                // end of for loop num_buckets_to_read

                // bufferShuffled needs to be sorted first before it is written
                //  Sort the bucket by computing hashes and extracting the sorted nonces.
                // make sure this works...
                // MemoRecord *sorted_nonces = sort_bucket_records(bufferShuffled, num_records_in_bucket*num_buckets_to_read*rounds);

                // should write in parallel if possible
                size_t elementsWritten = fwrite(bufferShuffled, sizeof(MemoRecord), num_records_in_bucket * num_buckets_to_read * rounds, fd_dest);
                // size_t elementsWritten = fwrite(sorted_nonces, sizeof(MemoRecord), num_records_in_bucket*num_buckets_to_read*rounds, fd_dest);
                if (elementsWritten != num_records_in_bucket * num_buckets_to_read * rounds)
                {
                    fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                            elementsWritten, num_records_in_bucket * num_buckets_to_read * rounds);
                    fclose(fd_dest);
                    exit(EXIT_FAILURE);
                }

                /*if (fsync(fileno(fd_dest)) != 0) {
                    perror("Failed to fsync buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }*/

                double end_time_io2 = omp_get_wtime();
                elapsed_time_io2 = end_time_io2 - start_time_io2;
                elapsed_time_io2_total += elapsed_time_io2;
                double throughput_io2 = (num_records_in_bucket * num_buckets_to_read * rounds * NONCE_SIZE) / (elapsed_time_io2 * 1024 * 1024);
                if (!BENCHMARK)
                    printf("[%.2f] Shuffle %.2f%%: %.2f MB/s\n", omp_get_wtime() - start_time, (i + 1) * 100.0 / num_buckets, throughput_io2);
            }
            // end of for loop
            start_time_io = omp_get_wtime();

            // Flush and close the file
            if (writeData)
            {
                if (fflush(fd) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd);
                    return EXIT_FAILURE;
                }

                if (fsync(fileno(fd)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd);
                    return EXIT_FAILURE;
                }
                fclose(fd);
            }

            if (writeDataFinal)
            {
                if (fflush(fd_dest) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }

                if (fsync(fileno(fd_dest)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }

                fclose(fd_dest);

                remove_file(FILENAME);
            }

            free(buffer);
        }
        else if (writeDataTable2 && rounds == 1)
        {
            // Call the rename_file function
            if (move_file_overwrite(FILENAME, FILENAME_TABLE2) == 0)
            {
                if (!BENCHMARK)
                    printf("File renamed/moved successfully from '%s' to '%s'.\n", FILENAME, FILENAME_TABLE2);
            }
            else
            {
                printf("Error in moving file '%s' to '%s'.\n", FILENAME, FILENAME_TABLE2);
                return EXIT_FAILURE;
                // Error message already printed by rename_file via perror()
                // Additional handling can be done here if necessary
                // return 1;
            }
        }

// will need to check on MacOS with a spinning hdd if we need to call sync() to flush all filesystems
#ifdef __linux__

        if (writeData)
        {
            if (DEBUG)
                printf("Final flush in progress...\n");
            int fd2 = open(FILENAME_TABLE2, O_RDWR);
            if (fd2 == -1)
            {
                printf("Error opening file %s (#6)\n", FILENAME_TABLE2);

                perror("Error opening file");
                return EXIT_FAILURE;
            }

            // Sync the entire filesystem
            if (syncfs(fd2) == -1)
            {
                perror("Error syncing filesystem with syncfs");
                close(fd2);
                return EXIT_FAILURE;
            }
        }
#endif

        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        /*
            if (writeDataTable2 && rounds > 1)
            {
                //this is not implemented yet
                printf("writing table 2 not implemented with rounds > 1\n");
                return -1;
            }
            //has sufficient memory
            else if (writeDataTable2 && rounds == 1)
            {
                printf("generating table 2...\n");
                if (generate_table2(FILENAME_FINAL, FILENAME_TABLE2, num_threads) == 0)
                {
                    printf("success in generate_table2()\n");
                }
                else
                {
                    printf("failure in generate_table2()\n");
                }
            }
            */

        // End total time measurement
        double end_time = omp_get_wtime();
        double elapsed_time = end_time - start_time;

        // Calculate total throughput
        double total_throughput = (num_iterations / elapsed_time) / 1e6;
        if (!BENCHMARK)
        {
            printf("Total Throughput: %.2f MH/s  %.2f MB/s\n", total_throughput, total_throughput * NONCE_SIZE);
            printf("Total Time: %.6f seconds\n", elapsed_time);
        }
        else
        {
            printf("%s,%d,%lu,%d,%llu,%.2f,%zu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", approach, K, sizeof(MemoRecord), num_threads, MEMORY_SIZE_MB, file_size_gb, BATCH_SIZE, total_throughput, total_throughput * NONCE_SIZE, elapsed_time_hash_total, elapsed_time_io_total, elapsed_time_io2_total, elapsed_time - elapsed_time_hash_total - elapsed_time_io_total - elapsed_time_io2_total, elapsed_time, record_counts * 100.0 / (num_buckets * num_records_in_bucket));
            return 0;
        }
    }
    // end of HASHGEN

    omp_set_num_threads(num_threads);

    if (SEARCH && !SEARCH_BATCH)
    {
        // printf("search has not been implemented yet...\n");
        search_memo_records(FILENAME_TABLE2, SEARCH_STRING);
    }

    if (SEARCH_BATCH)
    {
        // printf("search has not been implemented yet...\n");
        search_memo_records_batch(FILENAME_TABLE2, BATCH_SIZE, PREFIX_SEARCH_SIZE);
    }

    // Call the function to count zero-value MemoRecords
    // printf("verifying efficiency of final stored file...\n");
    // count_zero_memo_records(FILENAME_FINAL);

    // Call the function to process MemoRecords
    if (VERIFY)
    {
        if (!BENCHMARK)
            printf("verifying sorted order by bucketIndex of final stored file...\n");
        // process_memo_records(FILENAME_FINAL,MEMORY_SIZE_bytes/sizeof(MemoRecord));
        // process_memo_records(FILENAME_FINAL,num_records_in_bucket*rounds);
        //  FILENAME_TABLE2
        process_memo_records_table2(FILENAME_TABLE2, num_records_in_bucket * rounds);
    }

    if (DEBUG)
        printf("SUCCESS!\n");
    return 0;
}
