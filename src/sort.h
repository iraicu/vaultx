#ifndef SORT_H
#define SORT_H

#include "globals.h"
#include "table1.h"

int compare_hash_wrapper(const void *a, const void *b);
void sort_bucket_records_inplace(MemoRecord *records, size_t count);

#endif // SORT_H
