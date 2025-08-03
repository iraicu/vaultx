#include "table2.h"
#include <stdio.h>

void findMatches() {

    // FIXME: How to use a good bucket distance
    // Ideas: Average distance, multiply expected distance by a constant
    uint64_t expected_distance = 1ULL << (64 - K);

#pragma omp parallel for schedule(static)
    for (unsigned long long b = 0; b < total_buckets; b += BATCH_SIZE) {

        unsigned long long end = b + BATCH_SIZE;
        if (end > total_buckets) {
            end = total_buckets;
        }

        for (unsigned long long k = b; k <= end; k++) {
            int n = buckets[k].count;
            if (n <= 1) {
                continue;
            }

            // Allocate temporary sort array
            MemoRecordWithHash* sortArray = malloc(n * sizeof(MemoRecordWithHash));
            if (!sortArray) {
                perror("malloc failed");
                exit(EXIT_FAILURE);
            }

            // Precompute hashes
            for (int i = 0; i < n; i++) {
                sortArray[i].record = &buckets[k].records[i];
                generateBlake3(sortArray[i].record->nonce, key, sortArray[i].hash);
            }

            // Sort by precomputed hashes
            qsort(sortArray, n, sizeof(MemoRecordWithHash), compare_hash_wrapper);

            // Pairwise distance checking on sorted hashes
            for (int i = 0; i + 1 < n; i++) {
                uint8_t* hash1 = sortArray[i].hash;
                int j = i + 1;

                while (j < n) {
                    uint8_t* hash2 = sortArray[j].hash;

                    uint64_t distance = compute_hash_distance(hash1, hash2, HASH_SIZE);
                    if (distance > expected_distance) {
                        break;
                    }

                    MemoTable2Record record;
                    memcpy(record.nonce1, sortArray[i].record->nonce, NONCE_SIZE);
                    memcpy(record.nonce2, sortArray[j].record->nonce, NONCE_SIZE);

                    uint8_t hash[HASH_SIZE];
                    generateBlake3Pair(record.nonce1, record.nonce2, key, hash);

                    size_t bucketIndex = getBucketIndex(hash);
                    insert_record2(table2, buckets_table2, &record, bucketIndex);

                    j++;
                }
            }

            // Clear bucket
            free(sortArray);
            memset(buckets[k].records, 0, sizeof(MemoRecord) * num_records_in_bucket);
        }
    }
}

int insert_record2(MemoTable2Record* table2, BucketTable2* buckets2, MemoTable2Record* record, size_t bucketIndex) {
    if (bucketIndex >= total_buckets) {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, total_buckets - 1);
        return 0;
    }

    BucketTable2* bucket = &buckets2[bucketIndex];
    size_t idx;

    // Atomically capture the current count and increment it
#pragma omp atomic capture
    {
        idx = bucket->count;
        bucket->count++;
    }

    // Check if there's room in the bucket
    if (idx < num_records_in_bucket) {
        memcpy(table2[bucketIndex * num_records_in_bucket + idx].nonce1, record->nonce1, NONCE_SIZE);
        memcpy(table2[bucketIndex * num_records_in_bucket + idx].nonce2, record->nonce2, NONCE_SIZE);
        return 1;
    } else {
        // Ensure count doesn't exceed the maximum allowed
        bucket->count = num_records_in_bucket;
        if (!bucket->full) {
#pragma omp atomic
            full_buckets_global++;
            bucket->full = true;
        }
        bucket->count_waste++;
        // Overflow handling can be added here if necessary.
        return 0;
    }
}

// FIXME:
uint64_t compute_hash_distance(const uint8_t* hash_output, const uint8_t* prev_hash, size_t hash_size) {
    // Ensure there are at least 8 bytes in the hash
    if (hash_size < 8) {
        fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
        return 0;
    }

    // Convert the first 8 bytes of each hash to a uint64_t
    uint64_t current = byteArrayToLongLong(hash_output, 8);
    uint64_t previous = byteArrayToLongLong(prev_hash, 8);

    // Subtract the previous hash value from the current one and return the result
    return current > previous ? current - previous : previous - current;
}

// FIXME: Dont write big buffer at once, write in batches based on data from varvara
void writeTable2(uint8_t* plot_id) {
    char FILENAME[256];
    snprintf(FILENAME, sizeof(FILENAME), "/%s/arnav/vaultx/plots/K%d_%s.plot", SOURCE, K, byteArrayToHexString(plot_id, 32));

    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1) {
        printf("Error opening file %s (#4)\n", FILENAME);
        perror("Error opening file");
        return;
    }

    size_t total_size = total_nonces * sizeof(MemoTable2Record);
    char* data_ptr = (char*)table2;
    size_t total_written = 0;
    const size_t chunk_size = 4 * 1024 * 1024;

    while (total_written < total_size) {
        size_t remaining = total_size - total_written;
        size_t to_write = remaining < chunk_size ? remaining : chunk_size;

        ssize_t bytes = write(fd, data_ptr + total_written, to_write);

        if (bytes < 0) {
            fprintf(stderr, "Error writing to file at offset %zu: %s\n", total_written, strerror(errno));
            close(fd);
            return;
        }

        total_written += bytes;
    }

    // if (fsync(fd) != 0) {
    //     perror("Failed to fsync buffer");
    //     close(fd);
    //     return;
    // }

    close(fd);
}