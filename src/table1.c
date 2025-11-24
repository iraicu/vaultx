#include "table1.h"
#include "crypto.h"

// Generate Table1
void generateHashes() {
    volatile int buckets_full = 0; // Shared flag
    full_buckets_global = 0;

    double hashgen_start = (ENABLE_DETAILED_METRICS) ? omp_get_wtime() : 0;

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
        uint64_t local_hashes = 0;
        uint64_t local_atomics = 0;
        double local_compute_time = 0;
        double local_insert_time = 0;

        for (unsigned long long j = n; j <= batch_end; j++) {
            // Generate Blake3 hash
            double compute_start = (ENABLE_DETAILED_METRICS) ? omp_get_wtime() : 0;
            memcpy(record.nonce, &j, NONCE_SIZE);
            generateBlake3(record.nonce, key, hash);
            if (ENABLE_DETAILED_METRICS)
                local_compute_time += omp_get_wtime() - compute_start;

            off_t bucketIndex = getBucketIndex(hash);

            double insert_start = (ENABLE_DETAILED_METRICS) ? omp_get_wtime() : 0;
            insert_record(buckets, &record, bucketIndex);
            if (ENABLE_DETAILED_METRICS)
                local_insert_time += omp_get_wtime() - insert_start;

            local_hashes++;
            local_atomics++;
        }

        if (ENABLE_DETAILED_METRICS)
        {
#pragma omp atomic
            global_metrics.hashgen.total_hashes_generated += local_hashes;
#pragma omp atomic
            global_metrics.hashgen.actual_compute_time += local_compute_time;
#pragma omp atomic
            global_metrics.hashgen.bucket_insert_time += local_insert_time;
#pragma omp atomic
            global_metrics.hashgen.atomic_operations += local_atomics;
        }

        if (full_buckets_global >= total_buckets) {
            buckets_full = 1;
        }
    }

    if (ENABLE_DETAILED_METRICS)
    {
        global_metrics.hashgen.total_hashgen_time = omp_get_wtime() - hashgen_start;
        global_metrics.hashgen.hash_throughput_MHps = global_metrics.hashgen.total_hashes_generated / 1e6 / global_metrics.hashgen.total_hashgen_time;
        if (global_metrics.hashgen.total_hashgen_time > 0)
            global_metrics.hashgen.compute_efficiency = global_metrics.hashgen.actual_compute_time / global_metrics.hashgen.total_hashgen_time;
        global_metrics.hashgen.full_buckets_count = full_buckets_global;
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
        if (ENABLE_DETAILED_METRICS)
        {
#pragma omp atomic
            global_metrics.hashgen.bucket_overflows++;
        }
        // Overflow handling can be added here if necessary.
    }
}