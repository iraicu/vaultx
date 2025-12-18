#include "sort.h"
#include "crypto.h"
#include "table1.h"
#include <stdlib.h>

// Comparator for qsort
int compare_hash_wrapper(const void *a, const void *b) {
  const MemoRecordWithHash *A = a;
  const MemoRecordWithHash *B = b;
  return memcmp(A->hash, B->hash, HASH_SIZE);
}

// Sort bucket records in-place by their hash values
void sort_bucket_records_inplace(MemoRecord *records, size_t count) {
  if (count <= 1)
    return;

  // Create temporary array with hash + record pairs
  MemoRecordWithHash *sortArray = malloc(count * sizeof(MemoRecordWithHash));
  if (!sortArray) {
    fprintf(stderr, "Error: Failed to allocate memory for sorting\n");
    return;
  }

  // Compute hashes for all records
  for (size_t i = 0; i < count; i++) {
    sortArray[i].record = &records[i];
    generate_hash(records[i].nonce, sortArray[i].hash);
  }

  // Sort by hash
  qsort(sortArray, count, sizeof(MemoRecordWithHash), compare_hash_wrapper);

  // Copy sorted records back to original array
  MemoRecord *temp = malloc(count * sizeof(MemoRecord));
  if (!temp) {
    fprintf(stderr, "Error: Failed to allocate memory for temporary records\n");
    free(sortArray);
    return;
  }

  for (size_t i = 0; i < count; i++) {
    memcpy(&temp[i], sortArray[i].record, sizeof(MemoRecord));
  }

  memcpy(records, temp, count * sizeof(MemoRecord));

  free(temp);
  free(sortArray);
}
