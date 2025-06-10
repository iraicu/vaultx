#include <stdio.h>
#include "table2.h"

// // Comparison function for qsort(), comparing the hash fields.
// int compare_memo_all_record(const void *a, const void *b)
// {
//     const MemoAllRecord *recA = (const MemoAllRecord *)a;
//     const MemoAllRecord *recB = (const MemoAllRecord *)b;
//     return memcmp(recA->hash, recB->hash, HASH_SIZE);
// }

// /**
//  * sort_bucket_records:
//  *   - Takes an array of unsorted MemoRecord entries (with only nonce values).
//  *   - Internally allocates an array of MemoAllRecord, copies the nonces,
//  *     computes their Blake3 hashes, and sorts them based on the hash values.
//  *   - Extracts and returns a new array of MemoRecord that contains only the sorted nonces.
//  *
//  * @param unsorted      Pointer to an array of MemoRecord.
//  * @param total_records Total number of records in the bucket.
//  * @return Pointer to a newly allocated array of MemoRecord with sorted nonce values.
//  */
// MemoRecord *sort_bucket_records(const MemoRecord *unsorted, size_t total_records)
// {
//     // Allocate a temporary array of MemoAllRecord.
//     MemoAllRecord *all_records = malloc(total_records * sizeof(MemoAllRecord));
//     if (!all_records)
//     {
//         perror("Error allocating memory for MemoAllRecord array");
//         exit(EXIT_FAILURE);
//     }

//     // Populate all_records by copying the nonce from each unsorted record
//     // and computing its hash.
//     for (size_t i = 0; i < total_records; i++)
//     {
//         memcpy(all_records[i].nonce, unsorted[i].nonce, NONCE_SIZE);
//         blake3_hasher hasher;
//         blake3_hasher_init(&hasher);
//         blake3_hasher_update(&hasher, all_records[i].nonce, NONCE_SIZE);
//         blake3_hasher_finalize(&hasher, all_records[i].hash, HASH_SIZE);
//     }

//     // Sort the MemoAllRecord array based on the computed hash values.
//     qsort(all_records, total_records, sizeof(MemoAllRecord), compare_memo_all_record);

//     // Allocate an array to hold the sorted nonces.
//     MemoRecord *sorted_nonces = malloc(total_records * sizeof(MemoRecord));
//     if (!sorted_nonces)
//     {
//         perror("Error allocating memory for sorted nonces");
//         free(all_records);
//         exit(EXIT_FAILURE);
//     }

//     // Extract the nonce values from the sorted MemoAllRecord array.
//     for (size_t i = 0; i < total_records; i++)
//     {
//         memcpy(sorted_nonces[i].nonce, all_records[i].nonce, NONCE_SIZE);
//     }

//     // Free the temporary MemoAllRecord array.
//     free(all_records);

//     return sorted_nonces;
// }

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
    blake3_hasher_init_keyed(&hasher, hashed_key);
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

// void insert_record_table2(BucketTable2 *buckets2, uint8_t nonce1[NONCE_SIZE], uint8_t nonce2[NONCE_SIZE], size_t bucket_index)
// {
//     if (bucket_index >= num_buckets)
//     {
//         fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucket_index, num_buckets - 1);
//         return;
//     }

//     BucketTable2 *bucket = &buckets2[bucket_index];
//     size_t idx;

// #pragma omp atomic capture
//     {
//         idx = bucket->count;
//         bucket->count++;
//     }

//     // Check if there's room in the bucket
//     if (idx < num_records_in_bucket)
//     {
//         memcpy(bucket->records[idx].nonce1, nonce1, NONCE_SIZE);
//         memcpy(bucket->records[idx].nonce2, nonce2, NONCE_SIZE);
//     }
//     else
//     {
//         // Bucket is full; handle overflow if necessary
//         // For now, we ignore overflow
//         bucket->count = num_records_in_bucket;
// #pragma omp critical
//         if (!bucket->full)
//         {
//             full_buckets_global++;
//             bucket->full = true;
//         }
//         bucket->count_waste++;
//     }
// }

// void flatten_buckets_to_buffer(BucketTable2 *buckets2, MemoTable2Record *buffer2)
// {
//     off_t offset = 0;

//     for (int i = 0; i < num_buckets; i++)
//     {
//         if (buckets2[i].count > num_records_in_bucket)
//         {
//             fprintf(stderr, "Bucket %d has more records than expected!\n", i);
//             buckets2[i].count = num_records_in_bucket;
//         }

//         for (int j = 0; j < buckets2[i].count; j++)
//         {
//             memcpy(&buffer2[offset + j], &buckets2[i].records[j], sizeof(MemoTable2Record));
//             offset++;
//         }
//     }
// }

// Function to generate Blake3 hash
void generate2Blake3(uint8_t *record_hash, MemoTable2Record *record, unsigned long long nonce1, unsigned long long nonce2)
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
    blake3_hasher_init_keyed(&hasher, hashed_key);
    blake3_hasher_update(&hasher, record->nonce1, NONCE_SIZE);
    blake3_hasher_update(&hasher, record->nonce2, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, record_hash, HASH_SIZE);
}

// Function to insert a record into a bucket
int insert_record2(BucketTable2 *buckets2, MemoTable2Record *record, size_t bucketIndex)
{
    if (bucketIndex >= total_num_buckets)
    {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, total_num_buckets - 1);
        return 0;
    }

    BucketTable2 *bucket = &buckets2[bucketIndex];
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
        return 1;
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
        return 0;
    }
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
    size_t total_recs_in_file = filesize / sizeof(MemoTable2Record);
    size_t num_buckets = (total_recs_in_file + BATCH_SIZE - 1) / BATCH_SIZE;
    rewind(file);

    // --- allocate one batch buffer ---
    MemoTable2Record *buffer = malloc(BATCH_SIZE * sizeof(MemoTable2Record));
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
        size_t records_read = fread(buffer, sizeof(MemoTable2Record), BATCH_SIZE, file);
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
                blake3_hasher_init_keyed(&hasher, hashed_key);
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
                printf("Zero Nonces: %zu, Condition Met: %zu, Not Met: %zu, Total Records: %zu\n",
                       zero_nonce_count, count_condition_met, count_condition_not_met, total_records);
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

uint64_t generate_table2(MemoRecord *sorted_nonces, size_t num_records_in_bucket)
{
    // printf("Generating Table2 with %zu records in the bucket...\n", num_records_in_bucket);
    uint64_t expected_distance = 1ULL << (64 - K);
    uint64_t hash_pass_count = 0;

    // num_records_in_bucket = num_records_in_shuffled_bucket = num_records_in_bucket * rounds
    for (size_t i = 0; i < num_records_in_bucket; ++i)
    {
        if (is_nonce_nonzero(sorted_nonces[i].nonce, NONCE_SIZE))
        {
            // Compute Blake3 hash for record i
            uint8_t hash_i[HASH_SIZE];
            blake3_hasher hasher;
            blake3_hasher_init_keyed(&hasher, hashed_key);
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
                blake3_hasher_init_keyed(&hasher, hashed_key);
                blake3_hasher_update(&hasher, sorted_nonces[j].nonce, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_j, HASH_SIZE);

                // Compute the distance between hash_i and hash_j
                uint64_t distance = compute_hash_distance(hash_i, hash_j, HASH_SIZE);

                // Because data is sorted, break out of the inner loop once the distance exceeds expected_distance
                if (distance > expected_distance)
                {
                    break;
                }

                MemoTable2Record record;
                uint8_t hash_table2[HASH_SIZE];
                generate2Blake3(hash_table2, &record, (unsigned long long)sorted_nonces[i].nonce, (unsigned long long)sorted_nonces[j].nonce);

                if (MEMORY_WRITE)
                {
                    off_t bucketIndex = getBucketIndex(hash_table2);
                    hash_pass_count += insert_record2(buckets2, &record, bucketIndex);
                }
                // buckets2_count[bucketIndex]++;
                // printf("bucketIndex=%ld\n",bucketIndex);

                // print_table2_entry(record.nonce1, record.nonce2, hash_i, hash_j, HASH_SIZE);

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
    return hash_pass_count;
    // printf("hash_pass_count=%lu\ncompared to num_records_in_bucket(unshuffled bucket)=%llu\n\n", hash_pass_count, num_records_in_bucket / rounds);
}
