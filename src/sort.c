#include "sort.h"

// Comparison function for qsort(), comparing the hash fields.
// int compare_memo_all_record(const void *a, const void *b)
// {
//     const MemoAllRecord *recA = (const MemoAllRecord *)a;
//     const MemoAllRecord *recB = (const MemoAllRecord *)b;
//     return memcmp(recA->hash, recB->hash, HASH_SIZE);
// }

int compare_memo_all_record(const void* a, const void* b) {
    const MemoRecord* A = (const MemoRecord*)a;
    const MemoRecord* B = (const MemoRecord*)b;

    uint8_t hashA[HASH_SIZE];
    uint8_t hashB[HASH_SIZE];

    g(A->nonce, &current_file, hashA);
    g(B->nonce, &current_file, hashB);


    return memcmp(hashA, hashB, HASH_SIZE);
}

void sort_bucket_records_inplace(MemoRecord* records, size_t total_records) {
    // Build an auxiliary array of (nonce, hash) pairs
    MemoAllRecord* all_records = malloc(total_records * sizeof(MemoAllRecord));
    if (!all_records) {
        perror("Error allocating memory for MemoAllRecord array");
        exit(EXIT_FAILURE);
    }

    // Populate it
    for (size_t i = 0; i < total_records; i++) {
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
    for (size_t i = 0; i < total_records; i++) {
        memcpy(records[i].nonce, all_records[i].nonce, NONCE_SIZE);
    }

    // Clean up
    free(all_records);
}