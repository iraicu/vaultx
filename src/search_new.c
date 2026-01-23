#include "search.h"

// Utility: map a global record index into its owning file slice.
static int find_file_for_index(size_t idx, const size_t *prefix_offsets,
                               const size_t *effective_counts, int file_count) {
  for (int i = 0; i < file_count; i++) {
    size_t start = prefix_offsets[i];
    size_t end = start + effective_counts[i];
    if (idx >= start && idx < end) {
      return i;
    }
  }
  return -1;
}

bool search_rewrite_lookup(const uint8_t *query, size_t search_length,
                           SearchFileCtx *ctx_list, int file_count,
                           int io_threads, int hash_threads,
                           SearchMatch **matches_out,
                           size_t *match_count_out,
                           size_t *records_hashed_out,
                           size_t *matches_by_file,
                           double *io_ms_out, double *hash_ms_out,
                           double *total_ms_out) {
  if (!query || !ctx_list || file_count <= 0) {
    return false;
  }

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  // Compute total bucket capacity across all files.
  size_t total_capacity = 0;
  for (int i = 0; i < file_count; i++) {
    total_capacity += ctx_list[i].num_records_in_bucket_search;
  }
  if (total_capacity == 0) {
    return false;
  }

  MemoTable2Record *combined_buffer =
      (MemoTable2Record *)malloc(total_capacity * sizeof(MemoTable2Record));
  size_t *file_offsets = (size_t *)calloc(file_count, sizeof(size_t));
  size_t *effective_counts = (size_t *)calloc(file_count, sizeof(size_t));

  if (!combined_buffer || !file_offsets || !effective_counts) {
    free(combined_buffer);
    free(file_offsets);
    free(effective_counts);
    return false;
  }


  file_offsets[0] = 0;
  for (int i = 1; i < file_count; i++) {
    file_offsets[i] = file_offsets[i - 1] + ctx_list[i - 1].num_records_in_bucket_search;
  }

  int io_t = (io_threads > 0) ? io_threads : 1;
  double io_start = omp_get_wtime();
  int io_error = 0;

#pragma omp parallel for schedule(static) num_threads(io_t) reduction(+ : io_error)
  for (int i = 0; i < file_count; i++) {
    size_t records_read = 0;
    size_t effective_read = 0;
    bool ok = read_bucket_into_buffer(&ctx_list[i], query, search_length,
                                      &records_read, &effective_read);
    if (!ok) {
      io_error += 1;
      continue;
    }

    size_t offset = file_offsets[i];
    memcpy(combined_buffer + offset, ctx_list[i].buffer,
           records_read * sizeof(MemoTable2Record));
    effective_counts[i] = effective_read;
  }

  double io_ms = (omp_get_wtime() - io_start) * 1000.0;
  if (io_ms_out)
    *io_ms_out = io_ms;

  if (io_error > 0) {
    free(combined_buffer);
    free(file_offsets);
    free(effective_counts);
    return false;
  }

  size_t total_effective = 0;
  for (int i = 0; i < file_count; i++) {
    total_effective += effective_counts[i];
  }

  // Allocate match buffer with an initial capacity; grow as needed.
  SearchMatch *matches = NULL;
  size_t matches_capacity = 128;
  if (matches_capacity > 0) {
    matches = (SearchMatch *)malloc(matches_capacity * sizeof(SearchMatch));
  }

  size_t total_matches = 0;
  size_t total_hashed = 0;

  int hash_t = (hash_threads > 0) ? hash_threads : 1;
  double hash_start = omp_get_wtime();

#pragma omp parallel num_threads(hash_t)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t global_idx = 0; global_idx < total_effective; global_idx++) {
        int file_index = find_file_for_index(global_idx, file_offsets,
                                             effective_counts, file_count);
      if (file_index < 0)
        continue;

      size_t local_idx = global_idx - file_offsets[file_index];
      MemoTable2Record *rec = combined_buffer + file_offsets[file_index] + local_idx;

      if (is_record_empty(rec)) {
        continue;
      }

      uint8_t *record_key = ctx_list[file_index].local_key;
      if (ctx_list[file_index].plotData_array != NULL &&
          ctx_list[file_index].num_files > 0 &&
          ctx_list[file_index].records_per_file > 0) {
        int sub_file = (int)(local_idx / ctx_list[file_index].records_per_file);
        if (sub_file < ctx_list[file_index].num_files) {
          record_key = ctx_list[file_index].plotData_array[sub_file].key;
        }
      }

      generateBlake3Pair(rec->nonce1, rec->nonce2, record_key, hash_output);

#pragma omp atomic
      total_hashed++;

      if (memcmp(hash_output, query, search_length) == 0) {
        size_t slot = 0;
#pragma omp atomic capture
        slot = total_matches++;

        // Grow matches buffer if needed (single-threaded critical section)
        if (matches) {
          if (slot >= matches_capacity) {
#pragma omp critical
            {
              if (slot >= matches_capacity) {
                size_t new_cap = matches_capacity * 2;
                if (new_cap < slot + 1)
                  new_cap = slot + 1;
                SearchMatch *resized = (SearchMatch *)realloc(
                    matches, new_cap * sizeof(SearchMatch));
                if (resized) {
                  matches = resized;
                  matches_capacity = new_cap;
                }
              }
            }
          }
          if (slot < matches_capacity) {
            matches[slot].record = *rec;
            matches[slot].file_index = file_index;
          }
        }

        if (matches_by_file) {
#pragma omp atomic
          matches_by_file[file_index]++;
        }
      }
    }
  }

  double hash_ms = (omp_get_wtime() - hash_start) * 1000.0;
  if (hash_ms_out)
    *hash_ms_out = hash_ms;
  if (total_ms_out)
    *total_ms_out = io_ms + hash_ms;

  if (match_count_out)
    *match_count_out = total_matches;
  if (records_hashed_out)
    *records_hashed_out = total_hashed;

  if (matches_out)
    *matches_out = matches;

  // Caller owns matches array when returned.
  if (!matches_out && matches) {
    free(matches);
  }

  free(combined_buffer);
  free(file_offsets);
  free(effective_counts);
  return true;
}

// Extended version with detailed timing breakdown (seek/read/hash)
bool search_rewrite_lookup_timed(const uint8_t *query, size_t search_length,
                                 SearchFileCtx *ctx_list, int file_count,
                                 int io_threads, int hash_threads,
                                 SearchMatch **matches_out,
                                 size_t *match_count_out,
                                 size_t *records_hashed_out,
                                 size_t *matches_by_file,
                                 SearchTimingBreakdown *timing_out) {
  if (!query || !ctx_list || file_count <= 0) {
    return false;
  }

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  // Initialize timing breakdown
  SearchTimingBreakdown timing = {0.0, 0.0, 0.0, 0.0, 0.0};
  double overall_start = omp_get_wtime();

  // Compute total bucket capacity across all files.
  size_t total_capacity = 0;
  for (int i = 0; i < file_count; i++) {
    total_capacity += ctx_list[i].num_records_in_bucket_search;
  }
  if (total_capacity == 0) {
    return false;
  }

  MemoTable2Record *combined_buffer =
      (MemoTable2Record *)malloc(total_capacity * sizeof(MemoTable2Record));
  size_t *file_offsets = (size_t *)calloc(file_count, sizeof(size_t));
  size_t *effective_counts = (size_t *)calloc(file_count, sizeof(size_t));
  double *per_file_seek_ms = (double *)calloc(file_count, sizeof(double));
  double *per_file_read_ms = (double *)calloc(file_count, sizeof(double));

  if (!combined_buffer || !file_offsets || !effective_counts ||
      !per_file_seek_ms || !per_file_read_ms) {
    free(combined_buffer);
    free(file_offsets);
    free(effective_counts);
    free(per_file_seek_ms);
    free(per_file_read_ms);
    return false;
  }

  file_offsets[0] = 0;
  for (int i = 1; i < file_count; i++) {
    file_offsets[i] = file_offsets[i - 1] + ctx_list[i - 1].num_records_in_bucket_search;
  }

  int io_t = (io_threads > 0) ? io_threads : 1;
  int io_error = 0;

#pragma omp parallel for schedule(static) num_threads(io_t) reduction(+ : io_error)
  for (int i = 0; i < file_count; i++) {
    size_t records_read = 0;
    size_t effective_read = 0;
    double seek_ms = 0.0, read_ms = 0.0;
    bool ok = read_bucket_into_buffer_timed(&ctx_list[i], query, search_length,
                                            &records_read, &effective_read,
                                            &seek_ms, &read_ms);
    per_file_seek_ms[i] = seek_ms;
    per_file_read_ms[i] = read_ms;

    if (!ok) {
      io_error += 1;
      continue;
    }

    size_t offset = file_offsets[i];
    memcpy(combined_buffer + offset, ctx_list[i].buffer,
           records_read * sizeof(MemoTable2Record));
    effective_counts[i] = effective_read;
  }

  // Aggregate seek and read times across all files
  // We report sum for detailed breakdown to show total CPU time spent.
  double sum_seek_ms = 0.0, sum_read_ms = 0.0;
  for (int i = 0; i < file_count; i++) {
    sum_seek_ms += per_file_seek_ms[i];
    sum_read_ms += per_file_read_ms[i];
  }
  timing.seek_ms = sum_seek_ms;
  timing.read_ms = sum_read_ms;

  free(per_file_seek_ms);
  free(per_file_read_ms);

  if (io_error > 0) {
    free(combined_buffer);
    free(file_offsets);
    free(effective_counts);
    return false;
  }

  size_t total_effective = 0;
  for (int i = 0; i < file_count; i++) {
    total_effective += effective_counts[i];
  }

  // Allocate match buffer with an initial capacity; grow as needed.
  SearchMatch *matches = NULL;
  size_t matches_capacity = 128;
  if (matches_capacity > 0) {
    matches = (SearchMatch *)malloc(matches_capacity * sizeof(SearchMatch));
  }

  size_t total_matches = 0;
  size_t total_hashed = 0;

  int hash_t = (hash_threads > 0) ? hash_threads : 1;
  double hash_start = omp_get_wtime();

#pragma omp parallel num_threads(hash_t)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t global_idx = 0; global_idx < total_effective; global_idx++) {
      int file_index = find_file_for_index(global_idx, file_offsets,
                                           effective_counts, file_count);
      if (file_index < 0)
        continue;

      size_t local_idx = global_idx - file_offsets[file_index];
      MemoTable2Record *rec = combined_buffer + file_offsets[file_index] + local_idx;

      if (is_record_empty(rec)) {
        continue;
      }

      uint8_t *record_key = ctx_list[file_index].local_key;
      if (ctx_list[file_index].plotData_array != NULL &&
          ctx_list[file_index].num_files > 0 &&
          ctx_list[file_index].records_per_file > 0) {
        int sub_file = (int)(local_idx / ctx_list[file_index].records_per_file);
        if (sub_file < ctx_list[file_index].num_files) {
          record_key = ctx_list[file_index].plotData_array[sub_file].key;
        }
      }

      generateBlake3Pair(rec->nonce1, rec->nonce2, record_key, hash_output);

#pragma omp atomic
      total_hashed++;

      if (memcmp(hash_output, query, search_length) == 0) {
        size_t slot = 0;
#pragma omp atomic capture
        slot = total_matches++;

        // Grow matches buffer if needed (single-threaded critical section)
        if (matches) {
          if (slot >= matches_capacity) {
#pragma omp critical
            {
              if (slot >= matches_capacity) {
                size_t new_cap = matches_capacity * 2;
                if (new_cap < slot + 1)
                  new_cap = slot + 1;
                SearchMatch *resized = (SearchMatch *)realloc(
                    matches, new_cap * sizeof(SearchMatch));
                if (resized) {
                  matches = resized;
                  matches_capacity = new_cap;
                }
              }
            }
          }
          if (slot < matches_capacity) {
            matches[slot].record = *rec;
            matches[slot].file_index = file_index;
          }
        }

        if (matches_by_file) {
#pragma omp atomic
          matches_by_file[file_index]++;
        }
      }
    }
  }

  double hash_end = omp_get_wtime();
  timing.hash_ms = (hash_end - hash_start) * 1000.0;
  timing.total_ms = (omp_get_wtime() - overall_start) * 1000.0;

  // Note: open_close_ms is tracked externally since files are opened before this call
  // The caller can measure that separately and add it to the breakdown

  if (timing_out) {
    *timing_out = timing;
  }

  if (match_count_out)
    *match_count_out = total_matches;
  if (records_hashed_out)
    *records_hashed_out = total_hashed;

  if (matches_out)
    *matches_out = matches;

  // Caller owns matches array when returned.
  if (!matches_out && matches) {
    free(matches);
  }

  free(combined_buffer);
  free(file_offsets);
  free(effective_counts);
  return true;
}
