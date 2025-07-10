#include "table1.h"
#include "crypto.h"

// Generate Table1
void generateHashes() {
    volatile int buckets_full = 0; // Shared flag
    full_buckets_global = 0;

    int count = 0;

#pragma omp parallel for schedule(static) shared(buckets_full)

    for (unsigned long long n = 0; n < total_nonces; n += BATCH_SIZE) {
        if (buckets_full) {
            continue;
            ;
        }

        unsigned long long batch_end = n + BATCH_SIZE;
        if (batch_end >= total_nonces) {
            batch_end = total_nonces;
        }

        MemoRecord record;
        uint8_t hash[HASH_SIZE];

        for (unsigned long long j = n; j <= batch_end; j++) {
            // Generate Blake3 hash
            memcpy(record.nonce, &j, NONCE_SIZE);
            generateBlake3(record.nonce, key, hash);

            off_t bucketIndex = getBucketIndex(hash);

            insert_record(buckets, &record, bucketIndex);
        }

        if (full_buckets_global >= total_buckets) {
            buckets_full = 1;
        }
    }
}

// Insert record into bucket
void insert_record(Bucket* buckets, MemoRecord* record, size_t bucketIndex) {
    if (bucketIndex >= total_buckets) {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, total_buckets - 1);
        return;
    }

    Bucket* bucket = &buckets[bucketIndex];
    size_t idx;

// Atomically capture the current count and increment it
#pragma omp atomic capture
    {
        idx = bucket->count;
        bucket->count++;
    }

    // Check if there's room in the bucket
    if (idx < num_records_in_bucket) {
        memcpy(bucket->records[idx].nonce, record->nonce, NONCE_SIZE);
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
    }
}