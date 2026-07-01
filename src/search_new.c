#include "search.h"


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

  if (!combined_buffer || !file_offsets) {
    free(combined_buffer);
    free(file_offsets);
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
  }

  double io_ms = (omp_get_wtime() - io_start) * 1000.0;
  if (io_ms_out)
    *io_ms_out = io_ms;

  if (io_error > 0) {
    free(combined_buffer);
    free(file_offsets);
    return false;
  }

  // Precompute which file each physical slot belongs to for O(1) lookup.
  // Iterate over total_capacity (physical slots) so that empty slots between
  // effective and capacity boundaries in earlier files do not cause later
  // files' records to be skipped.
  int *slot_file = (int *)malloc(total_capacity * sizeof(int));
  if (!slot_file) {
    free(combined_buffer);
    free(file_offsets);
    return false;
  }
  {
    int fi = 0;
    for (size_t pi = 0; pi < total_capacity; pi++) {
      while (fi < file_count - 1 &&
             pi >= file_offsets[fi] + ctx_list[fi].num_records_in_bucket_search) {
        fi++;
      }
      slot_file[pi] = fi;
    }
  }

  // Pre-allocate to total_capacity
  SearchMatch *matches = (SearchMatch *)malloc(total_capacity * sizeof(SearchMatch));
  size_t matches_capacity = total_capacity;

  size_t total_matches = 0;
  size_t total_hashed = 0;

  int hash_t = (hash_threads > 0) ? hash_threads : 1;
  double hash_start = omp_get_wtime();

#pragma omp parallel num_threads(hash_t)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t phys_idx = 0; phys_idx < total_capacity; phys_idx++) {
      MemoTable2Record *rec = combined_buffer + phys_idx;
      if (is_record_empty(rec))
        continue;

      int file_index = slot_file[phys_idx];
      size_t local_idx = phys_idx - file_offsets[file_index];

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

        if (matches && slot < matches_capacity) {
          matches[slot].record = *rec;
          matches[slot].file_index = file_index;
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

  free(slot_file);
  free(combined_buffer);
  free(file_offsets);
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
  double *per_file_seek_ms = (double *)calloc(file_count, sizeof(double));
  double *per_file_read_ms = (double *)calloc(file_count, sizeof(double));

  if (!combined_buffer || !file_offsets ||
      !per_file_seek_ms || !per_file_read_ms) {
    free(combined_buffer);
    free(file_offsets);
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
  }

  // Aggregate seek and read times across all files.
  // These are cumulative CPU-time sums (all threads combined); the caller
  // converts them to proportional wall-clock fractions for reporting.
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
    return false;
  }

  // Precompute which file each physical slot belongs to for O(1) lookup.
  // We iterate over total_capacity (not total_effective) so that empty slots
  // inside a file's bucket section do not shift the physical positions of
  // records from later files, which would cause tail records to be missed.
  int *slot_file = (int *)malloc(total_capacity * sizeof(int));
  if (!slot_file) {
    free(combined_buffer);
    free(file_offsets);
    return false;
  }
  {
    int fi = 0;
    for (size_t pi = 0; pi < total_capacity; pi++) {
      while (fi < file_count - 1 &&
             pi >= file_offsets[fi] + ctx_list[fi].num_records_in_bucket_search) {
        fi++;
      }
      slot_file[pi] = fi;
    }
  }

  // Pre-allocate to total_capacity (same reasoning as search_rewrite_lookup).
  SearchMatch *matches = (SearchMatch *)malloc(total_capacity * sizeof(SearchMatch));
  size_t matches_capacity = total_capacity;

  size_t total_matches = 0;
  size_t total_hashed = 0;

  int hash_t = (hash_threads > 0) ? hash_threads : 1;
  double hash_start = omp_get_wtime();

#pragma omp parallel num_threads(hash_t)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t phys_idx = 0; phys_idx < total_capacity; phys_idx++) {
      MemoTable2Record *rec = combined_buffer + phys_idx;
      if (is_record_empty(rec))
        continue;

      int file_index = slot_file[phys_idx];
      size_t local_idx = phys_idx - file_offsets[file_index];

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

        if (matches && slot < matches_capacity) {
          matches[slot].record = *rec;
          matches[slot].file_index = file_index;
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

  free(slot_file);
  free(combined_buffer);
  free(file_offsets);
  return true;
}
