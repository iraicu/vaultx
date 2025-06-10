#include "table1.h"

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
    blake3_hasher_init_keyed(&hasher, hashed_key);
    blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, record_hash, HASH_SIZE);
}

// Function to insert a record into a bucket
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex)
{
    if (bucketIndex >= total_num_buckets)
    {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, total_num_buckets - 1);
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

// Recursive function to generate numbers using divide and conquer
void generateHashes(unsigned long long start, unsigned long long end)
{
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
                off_t bucketIndex = getBucketIndex(record_hash);
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

size_t process_memo_records(const char *filename, const size_t BATCH_SIZE)
{
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

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        double start_time_verify = omp_get_wtime();
        double end_time_verify = omp_get_wtime();
        bucket_not_full = false;
        // uint64_t distance = 0;

        // Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            ++total_records;

            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                uint8_t hash_output[HASH_SIZE];

                // Compute Blake3 hash of the nonce
                blake3_hasher hasher;
                blake3_hasher_init_keyed(&hasher, hashed_key);
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

                // Update the previous hash prefix and nonce
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
           count_condition_met, count_condition_not_met, zero_nonce_count, total_records, full_buckets, count_condition_met * 100.0 / total_records, full_buckets * 100.0 / (total_num_buckets));

    return count_condition_met;
}