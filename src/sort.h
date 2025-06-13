#ifndef SORT_H
#define SORT_H

#include "globals.h"
#include "table1.h"

int compare_memo_all_record(const void *a, const void *b);
void sort_bucket_records_inplace(MemoRecord *records, size_t total_records);

#endif // SORT_H