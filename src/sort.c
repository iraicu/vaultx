#include "sort.h"
#include "table1.h"

// Comparator for qsort
int compare_hash_wrapper(const void *a, const void *b) {
  const MemoRecordWithHash *A = a;
  const MemoRecordWithHash *B = b;
  return memcmp(A->hash, B->hash, HASH_SIZE);
}
