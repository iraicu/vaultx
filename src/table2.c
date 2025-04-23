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

// uint64_t compute_hash_distance(const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size)
// {
//     // Ensure there are at least 8 bytes in the hash
//     if (hash_size < 8)
//     {
//         fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
//         return 0;
//     }

//     // Convert the first 8 bytes of each hash to a uint64_t
//     uint64_t current = byteArrayToLongLong(hash_output, 8);
//     uint64_t previous = byteArrayToLongLong(prev_hash, 8);

//     // Subtract the previous hash value from the current one and return the result
//     return previous - current;
// }

// int print_table2_entry(const uint8_t *nonce_output, const uint8_t *prev_nonce, const uint8_t *hash_output, const uint8_t *prev_hash, size_t hash_size)
// {
//     // Ensure there are at least 8 bytes in the hash
//     if (hash_size < 8)
//     {
//         fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
//         return -1;
//     }

//     // Convert the first 8 bytes of each hash to a uint64_t
//     // uint64_t current = byteArrayToLongLong(hash_output, 8);
//     // uint64_t previous = byteArrayToLongLong(prev_hash, 8);
//     // uint64_t distance = previous - current;

//     // Print debugging information
//     fprintf(stderr, "Debug Info:\n");

//     // Print the first 8 bytes of hash_output
//     fprintf(stderr, "hash_output (first 8 bytes): ");
//     for (size_t i = 0; i < 8; i++)
//     {
//         fprintf(stderr, "%02X ", hash_output[i]);
//     }
//     fprintf(stderr, "\n");

//     // Print the first 8 bytes of prev_hash
//     fprintf(stderr, "prev_hash (first 8 bytes): ");
//     for (size_t i = 0; i < 8; i++)
//     {
//         fprintf(stderr, "%02X ", prev_hash[i]);
//     }
//     fprintf(stderr, "\n");

//     // Print the computed 64-bit values and the difference
//     // fprintf(stderr, "current: %" PRIu64 "\n", current);
//     // fprintf(stderr, "previous: %" PRIu64 "\n", previous);
//     // fprintf(stderr, "current - previous: %" PRIu64 "\n", distance);

//     uint8_t hash_table2[hash_size];
//     blake3_hasher hasher;
//     blake3_hasher_init(&hasher);
//     blake3_hasher_update(&hasher, prev_nonce, NONCE_SIZE);
//     blake3_hasher_update(&hasher, nonce_output, NONCE_SIZE);

//     blake3_hasher_finalize(&hasher, hash_table2, HASH_SIZE);

//     // Print the first 8 bytes of hash_table2
//     fprintf(stderr, "hash_table2 (first 8 bytes): ");
//     for (size_t i = 0; i < 8; i++)
//     {
//         fprintf(stderr, "%02X ", hash_table2[i]);
//     }
//     fprintf(stderr, "\n");

//     return 0;
// }

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

// size_t generate_table2(const char *filename, const char *filename_table2, int num_threads, int num_threads_io)
// {
//     MemoRecord *buffer = NULL;
//     MemoTable2Record *bufferTable2 = NULL;
//     // size_t total_records = 0;
//     // size_t zero_nonce_count = 0;
//     // size_t full_buckets = 0;
//     // bool bucket_not_full = false;
//     size_t records_read;
//     FILE *file = NULL;

//     // // Allocate memory for the array of BucketsTable2
//     buckets2 = (BucketTable2 *)calloc(num_buckets, sizeof(BucketTable2));
//     if (buckets2 == NULL)
//     {
//         fprintf(stderr, "Error: Unable to allocate memory for buckets2.\n");
//         exit(EXIT_FAILURE);
//     }

//     // Allocate memory for each bucket's records
//     for (unsigned long long i = 0; i < num_buckets; i++)
//     {
//         buckets2[i].records = (MemoTable2Record *)calloc(num_records_in_bucket, sizeof(MemoTable2Record));
//         if (buckets2[i].records == NULL)
//         {
//             fprintf(stderr, "Error: Unable to allocate memory for records.\n");
//             exit(EXIT_FAILURE);
//         }
//     }

//     int *buckets2_count;

//     buckets2_count = (int *)calloc(num_buckets, sizeof(int));
//     if (buckets2_count == NULL)
//     {
//         fprintf(stderr, "Error: Memory allocation failed in buckets2_count.\n");
//         return EXIT_FAILURE;
//     }

//     // Now maintain two previous hashes and nonces.
//     // uint8_t prev_hash[HASH_SIZE] = {0};         // immediate previous hash
//     // uint8_t prev_prev_hash[HASH_SIZE] = {0};      // hash from two records back
//     // uint8_t prev_nonce[NONCE_SIZE] = {0};         // immediate previous nonce
//     // uint8_t prev_prev_nonce[NONCE_SIZE] = {0};      // nonce corresponding to prev_prev_hash

//     // size_t count_condition_met = 0;       // Counter for records meeting the condition
//     // size_t count_condition_not_met = 0;

//     long filesize = get_file_size(filename);
//     if (filesize != -1)
//     {
//         if (!BENCHMARK)
//             printf("Size of '%s' is %ld bytes.\n", filename, filesize);
//     }

//     if (DEBUG)
//     {
//         printf("num_records_in_bucket=%llu\n", num_records_in_bucket);
//         printf("num_buckets=%llu\n", num_buckets);
//         // printf("BATCH_SIZE=%llu\n", BATCH_SIZE);
//         printf("filesize=%ld\n", filesize);
//     }

//     file = fopen(filename, "rb");
//     if (file == NULL)
//     {
//         printf("Error opening file %s (#3)\n", filename);
//         perror("Error opening file");
//         return -1;
//     }

//     printf("allocating memory to read table 1...\n");

//     // Allocate memory for the batch of MemoRecords
//     buffer = (MemoRecord *)malloc(num_buckets * num_records_in_bucket * sizeof(MemoRecord));
//     if (buffer == NULL)
//     {
//         fprintf(stderr, "Error: Unable to allocate memory.\n");
//         fclose(file);
//         return -1;
//     }
//     // Allocate memory for the table2 records
//     bufferTable2 = (MemoTable2Record *)malloc(num_buckets * num_records_in_bucket * sizeof(MemoTable2Record));
//     if (bufferTable2 == NULL)
//     {
//         fprintf(stderr, "Error: Unable to allocate memory.\n");
//         fclose(file);
//         free(buffer);
//         return -1;
//     }

//     printf("reading table 1 from disk...\n");
//     records_read = fread(buffer, sizeof(MemoRecord), num_buckets * num_records_in_bucket, file);
//     if (records_read <= 0 || records_read != num_buckets * num_records_in_bucket)
//         return -1;

//     double start_time = omp_get_wtime();
//     // double end_time = omp_get_wtime();

//     if (num_threads > 0)
//     {
//         omp_set_num_threads(num_threads);
//     }

//     printf("processing all buckets from memory in parallel on %d threads...\n", num_threads);
// #pragma omp parallel for schedule(static)
//     for (unsigned long long b = 0; b < num_buckets; b++)
//     {
//         double start_time_verify = omp_get_wtime();
//         double end_time_verify = omp_get_wtime();
//         // bucket_not_full = false;
//         // uint64_t leading_match = 0;
//         uint64_t expected_distance = 1ULL << (64 - K);
//         // uint64_t min_distance = UINT64_MAX;
//         // uint64_t max_distance = 0;
//         // uint64_t total_distance = 0;
//         // uint64_t count = 0;
//         uint64_t hash_pass_count = 0;

//         MemoRecord *sorted_nonces = sort_bucket_records(&buffer[b * num_records_in_bucket], num_records_in_bucket);

//         for (size_t i = 0; i < num_records_in_bucket; ++i)
//         {
//             //	total_records++;

//             if (is_nonce_nonzero(sorted_nonces[i].nonce, NONCE_SIZE))
//             {
//                 // Compute Blake3 hash for record i
//                 uint8_t hash_i[HASH_SIZE];
//                 blake3_hasher hasher;
//                 blake3_hasher_init(&hasher);
//                 blake3_hasher_update(&hasher, sorted_nonces[i].nonce, NONCE_SIZE);
//                 blake3_hasher_finalize(&hasher, hash_i, HASH_SIZE);

//                 // Compare hash_i with all subsequent non-zero nonce records
//                 // could change the upper bound here to be b+2 to span multiple buckets
//                 for (size_t j = i + 1; j < num_records_in_bucket; ++j)
//                 {
//                     // Skip records with zero nonce
//                     if (!is_nonce_nonzero(sorted_nonces[j].nonce, NONCE_SIZE))
//                     {
//                         continue;
//                     }

//                     // Compute Blake3 hash for record j
//                     uint8_t hash_j[HASH_SIZE];
//                     // blake3_hasher hasher_j;
//                     blake3_hasher_init(&hasher);
//                     blake3_hasher_update(&hasher, sorted_nonces[j].nonce, NONCE_SIZE);
//                     blake3_hasher_finalize(&hasher, hash_j, HASH_SIZE);

//                     // Compute the distance between hash_i and hash_j
//                     uint64_t distance = compute_hash_distance(hash_i, hash_j, HASH_SIZE);

//                     // Because data is sorted, break out of the inner loop once the distance exceeds expected_distance
//                     if (distance > expected_distance)
//                     {
//                         break;
//                     }

//                     // The distance is within the expected threshold; count it.
//                     hash_pass_count++;

//                     uint8_t hash_table2[HASH_SIZE];
//                     // blake3_hasher hasher_j;
//                     blake3_hasher_init(&hasher);
//                     blake3_hasher_update(&hasher, sorted_nonces[i].nonce, NONCE_SIZE);
//                     blake3_hasher_update(&hasher, sorted_nonces[j].nonce, NONCE_SIZE);
//                     blake3_hasher_finalize(&hasher, hash_table2, HASH_SIZE);

//                     off_t bucketIndex = getBucketIndex(hash_table2, PREFIX_SIZE);
//                     buckets2_count[bucketIndex]++;
//                     // printf("bucketIndex=%ld\n",bucketIndex);

//                     // Insert the record into appropriate BucketTable2
//                     insert_record_table2(buckets2, sorted_nonces[i].nonce, sorted_nonces[j].nonce, bucketIndex);

//                     // print_table2_entry(sorted_nonces[i].nonce, sorted_nonces[j].nonce, hash_i, hash_j, HASH_SIZE);

//                     // Optional: Print debug information every 1,048,576 records processed.
//                     // if (total_records % (1024 * 1024) == 0)
//                     //{
//                     //	printf("compute_hash_distance(): distance=%llu, expected_distance=%llu, hash_pass_count=%llu\n",
//                     //			distance, expected_distance,hash_pass_count);
//                     //}
//                 }
//                 // Count this nonzero nonce as processed.
//                 //	count++;
//             }
//             // else
//             //{
//             //	++zero_nonce_count;
//             //	bucket_not_full = true;
//             // }
//         }

//         // if (bucket_not_full == false)
//         //     full_buckets++;
//         end_time_verify = omp_get_wtime();

//         // hash_pass_count_global += hash_pass_count;
//         if (b % (1024 * 1024) == 0)
//         {
//             printf("[bucket %lld] Hashes passed: %lu out of %llu (%.2f%%)\n",
//                    b, hash_pass_count, num_records_in_bucket - 1, hash_pass_count * 100.0 / (num_records_in_bucket - 1));
//         }
//         double elapsed_time_verify = end_time_verify - start_time_verify;
//         double elapsed_time = omp_get_wtime() - start_time;
//         double throughput = (b * num_records_in_bucket * sizeof(MemoRecord) / elapsed_time_verify) / (1024 * 1024);
//         // if (b % (1024*1024) == 0)
//         //     printf("[%.2f] Verify %.2f%%: %.2f MB/s\n", elapsed_time,
//         //            b * 100.0 / num_buckets, throughput);

//         free(sorted_nonces);
//     }

//     if (ferror(file))
//     {
//         perror("Error reading file");
//     }
//     fclose(file);
//     free(buffer);

//     flatten_buckets_to_buffer(buckets2, bufferTable2);

//     // if (num_threads_io > 0)
//     // {
//     //     omp_set_num_threads(num_threads_io);
//     // }

//     // write table2 to disk
//     file = fopen(filename_table2, "wb");
//     if (file == NULL)
//     {
//         printf("Error opening file %s (#4)\n", filename_table2);
//         perror("Error opening file");
//         free(bufferTable2);
//         return -1;
//     }

//     size_t elements_written = fwrite(bufferTable2, sizeof(MemoTable2Record), num_buckets * num_records_in_bucket, file);
//     if (elements_written != num_buckets * num_records_in_bucket)
//     {
//         fprintf(stderr, "Error writing table2 to file; elements written %zu when expected %llu\n",
//                 elements_written, num_buckets * num_records_in_bucket);
//         fclose(file);
//         free(bufferTable2);
//         return -1;
//     }

//     fclose(file);
//     for (unsigned long long i = 0; i < num_buckets; i++)
//     {
//         free(buckets2[i].records);
//     }
//     free(buckets2);
//     free(bufferTable2);

//     // printf("sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu storage_efficiency=%.2f bucket_efficiency=%.2f\n",
//     //        count_condition_met, count_condition_not_met, zero_nonce_count, total_records, full_buckets,
//     //        count_condition_met * 100.0 / total_records, full_buckets * 100.0 / (num_buckets));

//     // 0 return code is success
//     unsigned long long table2_entries = 0;
//     // unsigned long long not_full_buckets = 0;
//     // unsigned long long full_buckets = 0;

//     unsigned long long entries_to_discard = 0;
//     unsigned long long table2_entries_discard = 0;

//     for (unsigned long long b = 0; b < num_buckets; b++)
//     {
//         table2_entries += buckets2_count[b];
//         if (buckets2_count[b] >= (int)num_records_in_bucket)
//         {
//             entries_to_discard = buckets2_count[b] - num_records_in_bucket;
//             table2_entries_discard += entries_to_discard;
//         }
//         // if (buckets2_count[b] >= num_records_in_bucket)
//         //	full_buckets++;
//         // else
//         //	not_full_buckets++;
//         // printf("buckets2_count[%lld]=%d\n",b,buckets2_count[b]);
//     }
//     free(buckets2_count);

//     printf("Total entries in table2 %lld minus discarded %lld; vault storage efficiency %.2f%%; missed opportunity %.2f%%\n", table2_entries, table2_entries_discard, (table2_entries - table2_entries_discard) * 100.0 / (num_buckets * num_records_in_bucket), (table2_entries_discard) * 100.0 / (num_buckets * num_records_in_bucket));

//     return 0;
// }

size_t process_memo_records_table2(
    const char *filename,
    const size_t BATCH_SIZE,
    int num_threads)
{
    // --- Open file & figure out total_records ---
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
    size_t total_records = filesize / sizeof(MemoTable2Record);
    rewind(file);

    // --- Load all records into memory ---
    MemoTable2Record *all = malloc(total_records * sizeof(MemoTable2Record));
    if (!all)
    {
        fprintf(stderr, "OOM allocating %zu records\n", total_records);
        fclose(file);
        return 0;
    }
    size_t got = fread(all, sizeof(MemoTable2Record), total_records, file);
    fclose(file);
    if (got != total_records)
    {
        fprintf(stderr, "Read only %zu of %zu records\n", got, total_records);
        free(all);
        return 0;
    }

    // --- Allocate buffers for parallel hash work ---
    uint8_t (*hashes)[HASH_SIZE] = malloc(total_records * HASH_SIZE);
    bool *valid = malloc(total_records * sizeof(bool));
    if (!hashes || !valid)
    {
        fprintf(stderr, "OOM allocating temp buffers\n");
        free(all);
        free(hashes);
        free(valid);
        return 0;
    }

    // --- Phase 1: compute every record's hash in parallel, with progress ---
    size_t hashed_count = 0;
    double hash_start_time = omp_get_wtime();
    double last_hash_print = hash_start_time;

#pragma omp parallel for num_threads(num_threads) schedule(static)
    for (size_t i = 0; i < total_records; i++)
    {
        if (is_nonce_nonzero(all[i].nonce1, NONCE_SIZE) && is_nonce_nonzero(all[i].nonce2, NONCE_SIZE))
        {
            valid[i] = true;
            blake3_hasher hasher;
            blake3_hasher_init(&hasher);
            blake3_hasher_update(&hasher, all[i].nonce1, NONCE_SIZE);
            blake3_hasher_update(&hasher, all[i].nonce2, NONCE_SIZE);
            blake3_hasher_finalize(&hasher, hashes[i], HASH_SIZE);
        }
        else
        {
            valid[i] = false;
        }

        // progress update
        size_t done;
#pragma omp atomic capture
        done = ++hashed_count;

        double now = omp_get_wtime();
        if (now - last_hash_print >= 1.0)
        {
#pragma omp critical
            {
                now = omp_get_wtime();
                if (now - last_hash_print >= 1.0)
                {
                    last_hash_print = now;
                    double pct = (double)done * 100.0 / (double)total_records;
                    printf("Hashing: %.2f%% (%zu/%zu)\n",
                           pct, done, total_records);
                }
            }
        }
    }
    // ensure final 100% print
    printf("Hashing: 100.00%% (%zu/%zu)\n", total_records, total_records);

    // --- Phase 2: serial prefix‐compare logic, with progress ---
    size_t count_condition_met = 0;
    size_t count_condition_not_met = 0;
    size_t zero_nonce_count = 0;
    size_t full_buckets = 0;
    size_t total_seen = 0;
    size_t num_buckets = (total_records + BATCH_SIZE - 1) / BATCH_SIZE;

    uint8_t prev_hash[HASH_SIZE] = {0};
    // uint8_t prev_nonce1[NONCE_SIZE] = {0};
    // uint8_t prev_nonce2[NONCE_SIZE] = {0};

    size_t compared_count = 0;
    double comp_start_time = omp_get_wtime();
    double last_comp_print_time = comp_start_time;

    for (size_t bucket = 0; bucket < num_buckets; bucket++)
    {
        bool bucket_not_full = false;
        size_t start = bucket * BATCH_SIZE;
        size_t end = start + BATCH_SIZE;
        if (end > total_records)
            end = total_records;

        for (size_t i = start; i < end; i++)
        {
            total_seen++;

            if (valid[i])
            {
                if (memcmp(hashes[i], prev_hash, PREFIX_SIZE) >= 0)
                {
                    count_condition_met++;
                }
                else
                {
                    count_condition_not_met++;
                    if (DEBUG)
                    {
                        // debug printing omitted for brevity
                    }
                }
                memcpy(prev_hash, hashes[i], HASH_SIZE);
                // memcpy(prev_nonce1, all[i].nonce1, NONCE_SIZE);
                // memcpy(prev_nonce2, all[i].nonce2, NONCE_SIZE);
            }
            else
            {
                zero_nonce_count++;
                bucket_not_full = true;
            }

            // progress update
            compared_count++;
            double now = omp_get_wtime();
            if (now - last_comp_print_time >= 1.0)
            {
                last_comp_print_time = now;
                double pct = (double)count_condition_met * 100.0 / (double)compared_count;
                printf("Comparing: %.2f%% (%zu/%zu)\n",
                       pct, count_condition_met, compared_count);
            }
        }

        if (!bucket_not_full)
        {
            full_buckets++;
        }
    }
    // ensure final 100% print
    double pct = (double)count_condition_met * 100.0 / (double)compared_count;
    printf("Comparing: %.2f%% (%zu/%zu)\n",
           pct, count_condition_met, compared_count);

    // --- Clean up ---
    free(all);
    free(hashes);
    free(valid);

    // --- Final summary ---
    double storage_eff = 100.0 * count_condition_met / total_seen;
    double bucket_eff = 100.0 * full_buckets / num_buckets;
    printf(
        "sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu full_buckets=%zu "
        "storage_efficiency=%.2f bucket_efficiency=%.2f\n",
        count_condition_met,
        count_condition_not_met,
        zero_nonce_count,
        total_seen,
        full_buckets,
        storage_eff,
        bucket_eff);

    return count_condition_met;
}