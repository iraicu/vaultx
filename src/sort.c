#include "sort.h"

// Comparison function for qsort(), comparing the hash fields.
int compare_memo_all_record(const void *a, const void *b)
{
    const MemoAllRecord *recA = (const MemoAllRecord *)a;
    const MemoAllRecord *recB = (const MemoAllRecord *)b;
    return memcmp(recA->hash, recB->hash, HASH_SIZE);
}


// why? 
// memory overhead
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
        generate_hash(all_records[i].nonce, all_records[i].hash);
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