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

typedef struct {
  char filename[256];
  FILE *file;
  long filesize;
  long data_filesize;
  unsigned long long num_buckets_search;
  unsigned long long num_records_in_bucket_search;
  PlotData *plotData_array;
  int num_files;
  int records_per_file;
  uint8_t local_key[32];
  MemoTable2Record *buffer;
} SearchFileCtx;

MemoTable2Record *search_memo_record(
    FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH,
    unsigned long long num_records_in_bucket_search, MemoTable2Record *buffer,
    int num_threads_bucket, PlotData *plotData, int total_files,
    int records_per_file, uint8_t *default_key);
SearchResult search_memo_records(const char *filename,
                                 const char *SEARCH_STRING,
                                 int num_threads_bucket);
SearchResult search_memo_records_batch(const char *filename, int num_lookups,
                                       int difficulty, int num_threads_bucket);

// Context-based API for single-query searches (supports keep-open semantics)
bool search_ctx_open(const char *filename, SearchFileCtx *ctx);
void search_ctx_close(SearchFileCtx *ctx);
SearchResult search_query_with_ctx(SearchFileCtx *ctx, const uint8_t *query,
                                   size_t search_length,
                                   int num_threads_bucket);

void print_buckets(const char *filename, int num_buckets_to_print);

#endif
