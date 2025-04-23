#ifndef SEARCH_H
#define SEARCH_H

#include "globals.h"

MemoTable2Record *search_memo_record(FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH, unsigned long long num_records_in_bucket_search, MemoTable2Record *buffer);
void search_memo_records(const char *filename, const char *SEARCH_STRING);
void search_memo_records_batch(const char *filename, int num_lookups, int search_size);

#endif