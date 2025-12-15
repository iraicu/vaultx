#ifndef SEARCH_H
#define SEARCH_H

#include "globals.h"
#include "vaultx.h"

typedef struct {
  char filename[256];
  long filesize;
  int num_lookups;
  int found_count;
  int not_found_count;
  double search_time_ms;
  double avg_time_per_lookup_ms;
} SearchResult;

MemoTable2Record *
search_memo_record(FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8,
                   size_t SEARCH_LENGTH,
                   unsigned long long num_records_in_bucket_search,
                   MemoTable2Record *buffer, int num_threads_bucket);
SearchResult search_memo_records(const char *filename,
                                 const char *SEARCH_STRING,
                                 int num_threads_bucket);
SearchResult search_memo_records_batch(const char *filename, int num_lookups,
                                       int difficulty, int num_threads_bucket);

#endif
