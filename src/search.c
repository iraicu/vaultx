#define _GNU_SOURCE // Must precede system headers (pulled in via search.h) for RUSAGE_THREAD
#include "search.h"
// Open a plot file and populate a reusable search context (supports merged plots).
bool search_ctx_open(const char *filename, SearchFileCtx *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);

  const char *basename = strrchr(filename, '/');
  basename = (basename == NULL) ? filename : basename + 1;

  ctx->file = fopen(filename, "rb");
  if (ctx->file == NULL) {
    perror("Error opening file");
    return false;
  }

  long filesize = get_file_size(filename);
  if (filesize <= 0) {
    fprintf(stderr, "Error: Invalid file size %ld for file '%s'\n", filesize,
            filename);
    fclose(ctx->file);
    ctx->file = NULL;
    return false;
  }
  ctx->filesize = filesize;
  ctx->data_filesize = filesize;

  // Extract K and plot id, handle merge suffix
  int k_value;
  char plot_id_string[65];
  uint8_t local_plot_id[32] = {0};
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files;
    if (sscanf(basename, "merge_%d_%d.plot", &k_value, &num_files) != 2) {
      fprintf(stderr,
              "Error: Invalid merge filename format '%s'. Expected merge_{K}_{N}.plot\n",
              basename);
      fclose(ctx->file);
      ctx->file = NULL;
      return false;
    }
    ctx->data_filesize = filesize - (num_files * sizeof(PlotData));
    memset(ctx->local_key, 0, 32);
    memset(plot_id_string, 0, sizeof(plot_id_string));
    memset(local_plot_id, 0, sizeof(local_plot_id));
  } else {
    char *dash = strchr(basename, '-');
    if (dash == NULL || dash - basename < 2) {
      fprintf(stderr,
              "Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
              basename);
      fclose(ctx->file);
      ctx->file = NULL;
      return false;
    }

    if (sscanf(basename, "k%d", &k_value) != 1) {
      fprintf(stderr, "Error: Could not parse K value from filename '%s'\n",
              basename);
      fclose(ctx->file);
      ctx->file = NULL;
      return false;
    }

    const char *hex_start = dash + 1;
    strncpy(plot_id_string, hex_start, sizeof(plot_id_string) - 1);
    plot_id_string[64] = '\0';

    char *dot = strchr(plot_id_string, '.');
    if (dot != NULL) {
      *dot = '\0';
    }

    if (hex_string_to_byte_array(plot_id_string, local_plot_id, 32) != 0) {
      fprintf(stderr,
              "Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 chars). Got '%s'\n",
              basename, plot_id_string);
      fclose(ctx->file);
      ctx->file = NULL;
      return false;
    }
  }

  derive_key(k_value, local_plot_id, ctx->local_key);

  ctx->num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  ctx->num_records_in_bucket_search =
      ctx->data_filesize / ctx->num_buckets_search / sizeof(MemoTable2Record);

  ctx->plotData_array = NULL;
  ctx->num_files = 0;
  ctx->footer_ms = 0.0;
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files_from_name;
    if (sscanf(basename, "merge_%*d_%d.plot", &num_files_from_name) == 1) {
      ctx->num_files = num_files_from_name;
      ctx->plotData_array =
          (PlotData *)malloc(ctx->num_files * sizeof(PlotData));
      if (ctx->plotData_array != NULL) {
        // Read the footer through a dedicated, throwaway fd instead of
        // fileno(ctx->file). pread() alone doesn't move ctx->file's stdio
        // position, but it's still a real read() on the SAME open file
        // description that the timed per-lookup fseek()/fread() in
        // read_bucket_into_buffer_timed() uses later - so it can still
        // influence that fd's kernel-side readahead/access-pattern state
        // before the timed region runs. A separate fd removes that
        // channel entirely: ctx->file is never touched here. The cost is
        // timed on its own (footer_ms) instead of being folded into
        // open_close_ms, so it stays visible in the breakdown rather than
        // silently explaining away a "missing" seek cost.
        size_t footer_bytes = (size_t)ctx->num_files * sizeof(PlotData);
        off_t footer_offset = (off_t)filesize - (off_t)footer_bytes;

        double footer_start = omp_get_wtime();
        int footer_fd = open(filename, O_RDONLY);
        ssize_t read_count = -1;
        if (footer_fd >= 0) {
          read_count = pread(footer_fd, ctx->plotData_array, footer_bytes,
                             footer_offset);
          close(footer_fd);
        }
        ctx->footer_ms = (omp_get_wtime() - footer_start) * 1000.0;

        if (read_count != (ssize_t)footer_bytes) {
          fprintf(stderr, "Warning: Failed to read metadata footer\n");
          free(ctx->plotData_array);
          ctx->plotData_array = NULL;
          ctx->num_files = 0;
        }
      }
    }
  }

  ctx->records_per_file = (ctx->num_files > 0)
                               ? (ctx->num_records_in_bucket_search / ctx->num_files)
                               : ctx->num_records_in_bucket_search;

  ctx->buffer = (MemoTable2Record *)malloc(ctx->num_records_in_bucket_search *
                                           sizeof(MemoTable2Record));
  if (ctx->buffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory.\n");
    if (ctx->plotData_array)
      free(ctx->plotData_array);
    fclose(ctx->file);
    ctx->file = NULL;
    return false;
  }

  // Rewind to start for subsequent bucket reads
  fseek(ctx->file, 0, SEEK_SET);

  return true;
}

// Release file handles and buffers associated with a search context.
void search_ctx_close(SearchFileCtx *ctx) {
  if (ctx->file) {
    fclose(ctx->file);
    ctx->file = NULL;
  }
  if (ctx->plotData_array) {
    free(ctx->plotData_array);
    ctx->plotData_array = NULL;
  }
  if (ctx->buffer) {
    free(ctx->buffer);
    ctx->buffer = NULL;
  }
}

// ---- Single lookup path ----

// Scan a single bucket, counting all matches and returning the first match (if any).
MemoTable2Record *search_memo_record(
    FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH,
    unsigned long long num_records_in_bucket_search, MemoTable2Record *buffer,
    int num_threads_bucket, PlotData *plotData, int total_files,
    int records_per_file, uint8_t *default_key, size_t *matches_found) {
  size_t records_read;
  MemoTable2Record *foundRecord = NULL;

  if (matches_found)
    *matches_found = 0;

  off_t offset = bucketIndex * (off_t)num_records_in_bucket_search *
                 (off_t)sizeof(MemoTable2Record);
  if (DEBUG)
    printf("SEARCH: seek to %" PRIuMAX " offset\n", (uintmax_t)offset);

  if (fseek(file, offset, SEEK_SET) != 0) {
    perror("Error seeking in file");
    return NULL;
  }

  records_read = fread(buffer, sizeof(MemoTable2Record),
                       num_records_in_bucket_search, file);
  if (records_read == 0) {
    printf("error reading from file..\n");
    return NULL;
  }

  if (DEBUG) {
    printf("\n=== SEARCH DEBUG ===\n");
    printf("Query (hex):     ");
    for (size_t n = 0; n < SEARCH_LENGTH; ++n)
      printf("%02X ", SEARCH_UINT8[n]);
    printf("\n");
    printf("Bucket Index:    %lld\n", (long long)bucketIndex);
    printf("Bucket Records:  %llu\n",
           (unsigned long long)num_records_in_bucket_search);
    printf("Comparing:       %zu bytes\n", SEARCH_LENGTH);
    printf("\nScanning bucket...\n");
  }

  size_t records_checked = 0;
  size_t first_match_index = (size_t)-1;
  size_t match_count = 0;

  size_t effective_records_read = records_read;
  if (plotData == NULL || total_files == 0 || records_per_file == 0) {
    for (size_t i = 0; i < records_read; ++i) {
      if (is_record_empty(&buffer[i])) {
        effective_records_read = i;
        break;
      }
    }
  }

  int threads = (num_threads_bucket > 0) ? num_threads_bucket : 1;

#pragma omp parallel if (threads > 1) num_threads(threads)                \
    reduction(+ : records_checked) reduction(+ : match_count)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t i = 0; i < effective_records_read; ++i) {
      if (plotData != NULL && total_files > 0 && records_per_file > 0 &&
          is_record_empty(&buffer[i])) {
        continue;
      }

      uint8_t *record_key = default_key;
      if (plotData != NULL && total_files > 0 && records_per_file > 0) {
        int file_index = (int)(i / records_per_file);
        if (file_index < total_files) {
          record_key = (uint8_t *)plotData[file_index].key;
        }
      }

      generateBlake3Pair(buffer[i].nonce1, buffer[i].nonce2, record_key,
                         hash_output);
      records_checked++;

      if (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0) {
        match_count++;
#pragma omp critical
        {
          if (first_match_index == (size_t)-1 || i < first_match_index) {
            first_match_index = i;
          }
        }

        if (DEBUG) {
          printf("[MATCH ] Query: ");
          for (size_t n = 0; n < SEARCH_LENGTH; ++n)
            printf("%02X ", SEARCH_UINT8[n]);
          printf("| Hash: ");
          for (size_t n = 0; n < SEARCH_LENGTH; ++n)
            printf("%02X ", hash_output[n]);
          printf("| Nonce1: ");
          for (size_t n = 0; n < NONCE_SIZE; ++n)
            printf("%02X", buffer[i].nonce1[n]);
          printf(" | Nonce2: ");
          for (size_t n = 0; n < NONCE_SIZE; ++n)
            printf("%02X", buffer[i].nonce2[n]);
          printf("\n");
        }
      } else if (DEBUG) {
        printf("[MISS  ] Query: ");
        for (size_t n = 0; n < SEARCH_LENGTH; ++n)
          printf("%02X ", SEARCH_UINT8[n]);
        printf("| Hash: ");
        for (size_t n = 0; n < SEARCH_LENGTH; ++n)
          printf("%02X ", hash_output[n]);
        printf("| Nonce1: ");
        for (size_t n = 0; n < NONCE_SIZE; ++n)
          printf("%02X", buffer[i].nonce1[n]);
        printf(" | Nonce2: ");
        for (size_t n = 0; n < NONCE_SIZE; ++n)
          printf("%02X", buffer[i].nonce2[n]);
        printf("\n");
      }
    }
  }

  if (matches_found)
    *matches_found = match_count;

  if (first_match_index != (size_t)-1) {
    foundRecord = &buffer[first_match_index];
  }

  if (DEBUG) {
    printf("\nBucket scan complete: %zu records checked, %s (matches=%zu)\n",
           records_checked, match_count > 0 ? "MATCH FOUND" : "NO MATCH",
           match_count);
    printf("===================\n\n");
  }

  return foundRecord;
}

// not sure if the search of more than PREFIX_LENGTH works
// CLI entry: open a plot file and perform a single user-supplied lookup.
SearchResult search_memo_records(const char *filename,
                                 const char *SEARCH_STRING,
                                 int num_threads_bucket) {
  SearchResult result = {0};
  const char *basename = strrchr(filename, '/');
  if (basename == NULL) {
    basename = filename;
  } else {
    basename++;
  }
  strncpy(result.filename, basename, sizeof(result.filename) - 1);
  result.num_lookups = 1;
  uint8_t local_key[32];
  uint8_t local_plot_id[32];
  uint8_t SEARCH_UINT8[HASH_SIZE] = {0};
  size_t SEARCH_LENGTH = strlen(SEARCH_STRING) / 2;
  if (SEARCH_LENGTH > HASH_SIZE) {
    SEARCH_LENGTH = HASH_SIZE;
  }

  if (hex_string_to_byte_array(SEARCH_STRING, SEARCH_UINT8, SEARCH_LENGTH) !=
      0) {
    printf("Error: Invalid search string '%s'. Expected %zu bytes.\n",
           SEARCH_STRING, SEARCH_LENGTH);
    return result;
  }
  off_t bucketIndex = getBucketIndex(SEARCH_UINT8);
  MemoTable2Record *buffer = NULL;

  FILE *file = NULL;

  long filesize = get_file_size(filename);
  long data_filesize = filesize;

  // Extract K value and hex plot ID from filename
  // Supports both formats: k{K}-{hex}.plot and merge_{K}_{N}.plot
  int k_value;
  char plot_id_string[65];
  if (strncmp(basename, "merge_", 6) == 0) {
    // Handle merge file format: merge_{K}_{N}.plot
    int num_files;
    if (sscanf(basename, "merge_%d_%d.plot", &k_value, &num_files) != 2) {
      printf("Error: Invalid merge filename format '%s'. Expected "
             "merge_{K}_{N}.plot\n",
             basename);
      return result;
    }
    data_filesize = filesize - (num_files * sizeof(PlotData));
    memset(local_key, 0, 32);
    memset(local_plot_id, 0, 32);
  } else {
    // Handle regular file format: k{K}-{hex_id}.plot
    char *dash = strchr(basename, '-');
    if (dash == NULL || dash - basename < 2) {
      printf(
          "Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
          basename);
      return result;
    }

    // Extract K value from k{K} prefix
    if (sscanf(basename, "k%d", &k_value) != 1) {
      printf("Error: Could not parse K value from filename '%s'\n", basename);
      return result;
    }

    // Start from the character after the dash
    const char *hex_start = dash + 1;
    strncpy(plot_id_string, hex_start, sizeof(plot_id_string) - 1);
    plot_id_string[64] = '\0';

    char *dot = strchr(plot_id_string, '.');
    if (dot != NULL) {
      *dot = '\0';
    }

    if (hex_string_to_byte_array(plot_id_string, local_plot_id, 32) != 0) {
      printf(
          "Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 "
          "chars). Got '%s'\n",
          basename, plot_id_string);
      return result;
    }
  }

  derive_key(k_value, local_plot_id, local_key);

  if (filesize != -1) {
    result.filesize = filesize;
    if (!BENCHMARK)
      printf("Size of '%s' is %ld bytes.\n", filename, filesize);
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      data_filesize / num_buckets_search / sizeof(MemoTable2Record);
  if (!BENCHMARK) {
    printf("SEARCH: filename=%s\n", filename);
    printf("SEARCH: filesize=%zu\n", filesize);
    printf("SEARCH: num_buckets=%lluu\n", num_buckets_search);
    printf("SEARCH: num_records_in_bucket=%llu\n",
           num_records_in_bucket_search);
    printf("SEARCH: SEARCH_STRING=%s\n", SEARCH_STRING);
  }

  // Open the file for reading in binary mode
  file = fopen(filename, "rb");
  if (file == NULL) {
    printf("Error opening file %s (#3)\n", filename);

    perror("Error opening file");
    return result;
  }

  // Allocate memory for the batch of MemoRecords
  buffer = (MemoTable2Record *)malloc(num_records_in_bucket_search *
                                      sizeof(MemoTable2Record));
  if (buffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory.\n");
    fclose(file);
    return result;
  }

  // Start walltime measurement
  double start_time = omp_get_wtime();
  // double end_time = omp_get_wtime();

  PlotData *plotData_array = NULL;
  int num_files = 0;
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files_from_name;
    if (sscanf(basename, "merge_%*d_%d.plot", &num_files_from_name) == 1) {
      num_files = num_files_from_name;
      plotData_array = (PlotData *)malloc(num_files * sizeof(PlotData));
      if (plotData_array != NULL) {
        // Seek to footer (at end of file)
        if (fseek(file, -(num_files * sizeof(PlotData)), SEEK_END) == 0) {
          size_t read_count =
              fread(plotData_array, sizeof(PlotData), num_files, file);
          if (read_count != (size_t)num_files) {
            fprintf(stderr, "Warning: Failed to read metadata footer\n");
            free(plotData_array);
            plotData_array = NULL;
            num_files = 0;
          }
        }
      }
    }
  }

  int records_per_file = (num_files > 0)
                             ? (num_records_in_bucket_search / num_files)
                             : num_records_in_bucket_search;
  size_t matches_found = 0;
  (void)search_memo_record(file, bucketIndex, SEARCH_UINT8, SEARCH_LENGTH,
                           num_records_in_bucket_search, buffer,
                           num_threads_bucket, plotData_array, num_files,
                           records_per_file, local_key, &matches_found);

  double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

  // Check for reading errors
  if (ferror(file)) {
    perror("Error reading file");
  }

  // Clean up
  fclose(file);

  result.search_time_ms = elapsed_time;
  result.avg_time_per_lookup_ms = elapsed_time;
  result.match_count = (long long)matches_found;
  if (matches_found > 0) {
    result.found_count = 1;
    result.not_found_count = 0;
  } else {
    result.found_count = 0;
    result.not_found_count = 1;
  }

  // Print all matches for this specific string
  if (matches_found > 0) {
    printf("%zu matches for HASH prefix %s:\n", matches_found,
           SEARCH_STRING);
    size_t effective_records_read = num_records_in_bucket_search;
    if (plotData_array == NULL || num_files == 0 || records_per_file == 0) {
      for (size_t i = 0; i < num_records_in_bucket_search; ++i) {
        if (is_record_empty(&buffer[i])) {
          effective_records_read = i;
          break;
        }
      }
    }

    for (size_t i = 0; i < effective_records_read; ++i) {
      if (plotData_array != NULL && num_files > 0 && records_per_file > 0 &&
          is_record_empty(&buffer[i])) {
        continue;
      }

      uint8_t *record_key = local_key;
      if (plotData_array != NULL && num_files > 0 && records_per_file > 0) {
        int file_index = (int)(i / records_per_file);
        if (file_index < num_files) {
          record_key = plotData_array[file_index].key;
        }
      }

      uint8_t hash_output[HASH_SIZE];
      generateBlake3Pair(buffer[i].nonce1, buffer[i].nonce2, record_key,
                         hash_output);

      if (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0) {
        printf("NONCE (");
        for (size_t n = 0; n < NONCE_SIZE; ++n)
          printf("%02X", buffer[i].nonce1[n]);
        printf(", ");
        for (size_t n = 0; n < NONCE_SIZE; ++n)
          printf("%02X", buffer[i].nonce2[n]);
        printf(")\n");
      }
    }
  } else {
    printf("no NONCE found for HASH prefix %s\n", SEARCH_STRING);
  }

  printf("search time %.2f ms\n", elapsed_time);

  // Clean up after any printing that relies on buffer/plotData_array.
  if (plotData_array != NULL) {
    free(plotData_array);
  }
  free(buffer);

  return result;
}

// ---- Bucket helpers ----

// Load the bucket matching the query prefix into the context buffer.
bool read_bucket_into_buffer(SearchFileCtx *ctx, const uint8_t *query,
                             size_t search_length, size_t *records_read,
                             size_t *effective_records_read) {
  if (!ctx || !ctx->file || !ctx->buffer || !records_read ||
      !effective_records_read) {
    return false;
  }

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  off_t bucketIndex = getBucketIndex(query);
  off_t offset = bucketIndex * (off_t)ctx->num_records_in_bucket_search *
                 (off_t)sizeof(MemoTable2Record);

  if (fseek(ctx->file, offset, SEEK_SET) != 0) {
    perror("Error seeking in file");
    return false;
  }

  size_t read_count = fread(ctx->buffer, sizeof(MemoTable2Record),
                            ctx->num_records_in_bucket_search, ctx->file);
  *records_read = read_count;
  size_t effective = read_count;

  if (ctx->plotData_array == NULL || ctx->num_files == 0 ||
      ctx->records_per_file == 0) {
    for (size_t i = 0; i < read_count; ++i) {
      if (is_record_empty(&ctx->buffer[i])) {
        effective = i;
        break;
      }
    }
  }

  *effective_records_read = effective;
  return true;
}

// Timed variant that returns seek and read times separately.
bool read_bucket_into_buffer_timed(SearchFileCtx *ctx, const uint8_t *query,
                                   size_t search_length, size_t *records_read,
                                   size_t *effective_records_read,
                                   double *seek_ms_out, double *read_ms_out) {
  if (!ctx || !ctx->file || !ctx->buffer || !records_read ||
      !effective_records_read) {
    return false;
  }

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  off_t bucketIndex = getBucketIndex(query);
  off_t offset = bucketIndex * (off_t)ctx->num_records_in_bucket_search *
                 (off_t)sizeof(MemoTable2Record);
  size_t want_bytes =
      (size_t)ctx->num_records_in_bucket_search * sizeof(MemoTable2Record);
  int fd = fileno(ctx->file);

  // History: fseek()/fread() used to split this into "seek" and "read"
  // timers, but strace -T showed lseek() itself only costs microseconds -
  // that split was actually measuring glibc's internal stdio buffer-fill
  // decision (want_bytes below vs. at/above the ~4KB stdio buffer takes a
  // slower "underflow" vs. a direct bypass path), not real seek time.
  // pread() below bypasses that buffer entirely.
  //
  // To still get an honest seek estimate, pay for the drive's actual
  // seek + rotational latency directly: read a single throwaway byte at
  // the target offset and time only that. POSIX_FADV_RANDOM stops the
  // kernel from speculatively reading ahead past the probe, which would
  // otherwise let it silently satisfy part (or, for small buckets, all)
  // of the real read from cache and erase the split we're trying to
  // measure. The full bucket is re-read from the same offset afterward,
  // so buffer layout and downstream record parsing are unaffected.
#if defined(__linux__)
  posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
#endif

  uint8_t probe_byte;
  double seek_start = omp_get_wtime();
  ssize_t probe_bytes = pread(fd, &probe_byte, 1, offset);
  double seek_end = omp_get_wtime();
  if (probe_bytes < 0) {
    perror("Error probing bucket offset");
    return false;
  }
  if (seek_ms_out) {
    *seek_ms_out = (seek_end - seek_start) * 1000.0;
  }

  double read_start = omp_get_wtime();
  ssize_t read_bytes = pread(fd, ctx->buffer, want_bytes, offset);
  double read_end = omp_get_wtime();
  if (read_ms_out) {
    *read_ms_out = (read_end - read_start) * 1000.0;
  }

  if (read_bytes < 0) {
    perror("Error reading bucket");
    return false;
  }
  size_t read_count = (size_t)read_bytes / sizeof(MemoTable2Record);

  *records_read = read_count;
  size_t effective = read_count;

  if (ctx->plotData_array == NULL || ctx->num_files == 0 ||
      ctx->records_per_file == 0) {
    for (size_t i = 0; i < read_count; ++i) {
      if (is_record_empty(&ctx->buffer[i])) {
        effective = i;
        break;
      }
    }
  }

  *effective_records_read = effective;
  return true;
}

// Hash bucket contents and check for a match using optional per-bucket threading.
size_t hash_bucket_buffer(const SearchFileCtx *ctx, const uint8_t *query,
                          size_t search_length, size_t effective_records,
                          int num_threads_bucket, size_t *records_checked,
                          size_t *matches_found) {
  if (!ctx || !ctx->buffer || !query) {
    return 0;
  }

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  size_t checked_total = 0;
  size_t match_total = 0;
  int threads = (num_threads_bucket > 0) ? num_threads_bucket : 1;

#pragma omp parallel if (threads > 1) num_threads(threads)                \
    reduction(+ : checked_total) reduction(+ : match_total)
  {
    uint8_t hash_output[HASH_SIZE];

#pragma omp for schedule(static)
    for (size_t i = 0; i < effective_records; ++i) {
      if (ctx->plotData_array != NULL && ctx->num_files > 0 &&
          ctx->records_per_file > 0 && is_record_empty(&ctx->buffer[i])) {
        continue;
      }

      uint8_t *record_key = (uint8_t *)ctx->local_key;
      if (ctx->plotData_array != NULL && ctx->num_files > 0 &&
          ctx->records_per_file > 0) {
        if (ctx->records_per_file > 0) {
          int file_index = (int)(i / ctx->records_per_file);
          if (file_index < ctx->num_files) {
            record_key = (uint8_t *)ctx->plotData_array[file_index].key;
          }
        }
      }

      generateBlake3Pair(ctx->buffer[i].nonce1, ctx->buffer[i].nonce2,
                         record_key, hash_output);
      checked_total++;

      if (memcmp(hash_output, query, search_length) == 0) {
        match_total++;
      }
    }
  }

  if (records_checked) {
    *records_checked = checked_total;
  }
  if (matches_found) {
    *matches_found = match_total;
  }

  return match_total;
}

// Execute a single lookup using an already-opened context and caller-provided query.
SearchResult search_query_with_ctx(SearchFileCtx *ctx, const uint8_t *query,
                                   size_t search_length,
                                   int num_threads_bucket) {
  SearchResult result = {0};
  snprintf(result.filename, sizeof(result.filename), "%s", ctx->filename);
  result.num_lookups = 1;
  result.filesize = ctx->filesize;

  if (search_length > HASH_SIZE) {
    search_length = HASH_SIZE;
  }

  size_t records_read = 0;
  size_t effective_records = 0;
  size_t records_checked = 0;
  size_t matches_found = 0;
  double start_time = omp_get_wtime();

  bool read_ok = read_bucket_into_buffer(ctx, query, search_length,
                                         &records_read, &effective_records);

  int found = 0;
  if (read_ok && effective_records > 0) {
    size_t match_total = hash_bucket_buffer(
        ctx, query, search_length, effective_records, num_threads_bucket,
        &records_checked, &matches_found);
    found = (match_total > 0) ? 1 : 0;
  }

  double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;
  result.search_time_ms = elapsed_time;
  result.avg_time_per_lookup_ms = elapsed_time;
  if (found) {
    result.found_count = 1;
    result.not_found_count = 0;
  } else {
    result.found_count = 0;
    result.not_found_count = 1;
  }
  result.match_count = (long long)matches_found;

  return result;
}

// ---- Batch lookup path ----

// Generate random queries and benchmark batch lookups against a plot file.
// not sure if the search of more than PREFIX_LENGTH works
SearchResult search_memo_records_batch(const char *filename, int num_lookups,
                                       int difficulty, int num_threads_bucket) {
  SearchResult result = {0};
  const char *basename = strrchr(filename, '/');
  if (basename == NULL) {
    basename = filename;
  } else {
    basename++;
  }
  strncpy(result.filename, basename, sizeof(result.filename) - 1);
  result.num_lookups = num_lookups;
  uint8_t local_key[32];
  uint8_t local_plot_id[32];

  srand((unsigned int)time(NULL));

  size_t SEARCH_LENGTH = (difficulty == 0) ? HASH_SIZE : difficulty;
  if (SEARCH_LENGTH > HASH_SIZE) {
    SEARCH_LENGTH = HASH_SIZE;
  }
  MemoTable2Record *buffer = NULL;

  FILE *file = NULL;
  int foundRecords = 0;
  int notFoundRecords = 0;
  long long all_matches = 0;

  long filesize = get_file_size(filename);
  result.filesize = filesize;
  long data_filesize = filesize;

  // Extract K value and derive key from filename
  // Supports both formats: k{K}-{hex}.plot and merge_{K}_{N}.plot
  int k_value;
  char plot_id_string[65];
  if (strncmp(basename, "merge_", 6) == 0) {
    // Handle merge file format: merge_{K}_{N}.plot
    int num_files;
    if (sscanf(basename, "merge_%d_%d.plot", &k_value, &num_files) != 2) {
      printf("Error: Invalid merge filename format '%s'. Expected "
             "merge_{K}_{N}.plot\n",
             basename);
      return result;
    }
    data_filesize = filesize - (num_files * sizeof(PlotData));
    memset(local_key, 0, 32);
    memset(local_plot_id, 0, 32);
  } else {
    // Handle regular file format: k{K}-{hex_id}.plot
    char *dash = strchr(basename, '-');
    if (dash == NULL || dash - basename < 2) {
      printf(
          "Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
          basename);
      return result;
    }

    // Extract K value from k{K} prefix
    if (sscanf(basename, "k%d", &k_value) != 1) {
      printf("Error: Could not parse K value from filename '%s'\n", basename);
      return result;
    }

    // Start from the character after the dash
    const char *hex_start = dash + 1;
    strncpy(plot_id_string, hex_start, sizeof(plot_id_string) - 1);
    plot_id_string[64] = '\0';

    char *dot = strchr(plot_id_string, '.');
    if (dot != NULL) {
      *dot = '\0';
    }

    if (hex_string_to_byte_array(plot_id_string, local_plot_id, 32) != 0) {
      printf(
          "Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 "
          "chars). Got '%s'\n",
          basename, plot_id_string);
      return result;
    }
  }

  derive_key(k_value, local_plot_id, local_key);

  if (filesize != -1) {
    if (!BENCHMARK)
      printf("Size of '%s' is %ld bytes.\n", filename, filesize);
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      data_filesize / num_buckets_search / sizeof(MemoTable2Record);
  if (!BENCHMARK) {
    printf("SEARCH: filename=%s\n", filename);
    printf("SEARCH: filesize=%zu\n", filesize);
    printf("SEARCH: num_buckets=%llu\n", num_buckets_search);
    printf("SEARCH: num_records_in_bucket=%llu\n",
           num_records_in_bucket_search);
    printf("SEARCH: difficulty=%d (matching %zu bytes)\n", difficulty,
           SEARCH_LENGTH);
  }

  file = fopen(filename, "rb");
  if (file == NULL) {
    printf("Error opening file %s (#3)\n", filename);

    perror("Error opening file");
    return result;
  }

  buffer = (MemoTable2Record *)malloc(num_records_in_bucket_search *
                                      sizeof(MemoTable2Record));
  if (buffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory.\n");
    fclose(file);
    return result;
  }

  PlotData *plotData_array = NULL;
  int num_files = 0;
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files_from_name;
    if (sscanf(basename, "merge_%*d_%d.plot", &num_files_from_name) == 1) {
      num_files = num_files_from_name;
      plotData_array = (PlotData *)malloc(num_files * sizeof(PlotData));
      if (plotData_array != NULL) {
        // Seek to footer (at end of file)
        if (fseek(file, -(num_files * sizeof(PlotData)), SEEK_END) == 0) {
          size_t read_count =
              fread(plotData_array, sizeof(PlotData), num_files, file);
          if (read_count != (size_t)num_files) {
            fprintf(stderr, "Warning: Failed to read metadata footer\n");
            free(plotData_array);
            plotData_array = NULL;
            num_files = 0;
          }
        }
      }
    }
  }

  double start_time = omp_get_wtime();

  uint8_t SEARCH_UINT8[HASH_SIZE] = {0};

  for (int i = 0; i < num_lookups; i++) {
    for (size_t j = 0; j < SEARCH_LENGTH; ++j) {
      SEARCH_UINT8[j] = rand() % 256;
    }

    // if (ENABLE_DETAILED_METRICS)
    // {
    //     double lookup_start = omp_get_wtime();
    //
    //     // Phase: Seek
    //     double seek_start = omp_get_wtime();
    //     fRecord = search_memo_record(file, getBucketIndex(SEARCH_UINT8),
    //     SEARCH_UINT8, SEARCH_LENGTH, num_records_in_bucket_search, buffer);
    //     global_metrics.lookup.file_seek_time += omp_get_wtime() - seek_start;
    //     global_metrics.lookup.io_seek_calls++;
    //
    //     global_metrics.lookup.total_lookup_time += omp_get_wtime() -
    //     lookup_start; global_metrics.lookup.bytes_read +=
    //     num_records_in_bucket_search * sizeof(MemoTable2Record);
    //     global_metrics.lookup.io_read_calls++;
    // }
    // else
    // {
    int records_per_file = (num_files > 0)
                               ? (num_records_in_bucket_search / num_files)
                               : num_records_in_bucket_search;
    size_t matches_found = 0;
    (void)search_memo_record(file, getBucketIndex(SEARCH_UINT8), SEARCH_UINT8,
                 SEARCH_LENGTH, num_records_in_bucket_search,
                 buffer, num_threads_bucket, plotData_array,
                 num_files, records_per_file, local_key,
                 &matches_found);
    // }
    if (matches_found > 0) {
      foundRecords++;
      all_matches += (long long)matches_found;
    } else {
      notFoundRecords++;
    }

    // if (ENABLE_DETAILED_METRICS)
    //     global_metrics.lookup.lookups_performed++;
  }

  double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

  // Check for reading errors
  if (ferror(file)) {
    perror("Error reading file");
  }

  // Clean up
  fclose(file);
  free(buffer);
  if (plotData_array != NULL) {
    free(plotData_array);
  }

  result.found_count = foundRecords;
  result.not_found_count = notFoundRecords;
  result.match_count = all_matches;
  result.search_time_ms = elapsed_time;
  result.avg_time_per_lookup_ms = elapsed_time / num_lookups;

  if (!BENCHMARK)
    printf("searched for %d lookups of %zu bytes long, found %d, not found %d "
           "in %.2f seconds, %.4f ms per lookup\n",
           num_lookups, SEARCH_LENGTH, foundRecords, notFoundRecords,
           elapsed_time / 1000.0, elapsed_time / num_lookups);
  else
    printf("%s %zu %llu %llu %d %zu %d %d %.2f %.2f\n", filename, filesize,
           num_buckets_search, num_records_in_bucket_search, num_lookups,
           SEARCH_LENGTH, foundRecords, notFoundRecords, elapsed_time / 1000.0,
           elapsed_time / num_lookups);

  return result;
}

// ---- Debug helpers ----

// Debug helper: print the first N buckets and verify prefix alignment.
void print_buckets(const char *filename, int num_buckets_to_print) {
  const char *basename = strrchr(filename, '/');
  if (basename == NULL) {
    basename = filename;
  } else {
    basename++;
  }

  printf("DEBUG: print_buckets called with filename='%s', num_buckets=%d\n",
         filename, num_buckets_to_print);
  printf("DEBUG: sizeof(MemoTable2Record)=%zu, NONCE_SIZE=%d\n",
         sizeof(MemoTable2Record), NONCE_SIZE);

  uint8_t local_key[32];
  uint8_t local_plot_id[32];
  FILE *file = NULL;

  long filesize = get_file_size(filename);
  if (filesize <= 0) {
    fprintf(stderr, "Error: Invalid file size %ld for file '%s'\n", filesize,
            filename);
    return;
  }

  long data_filesize = filesize;

  int k_value;
  char plot_id_string[65];
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files;
    if (sscanf(basename, "merge_%d_%d.plot", &k_value, &num_files) != 2) {
      printf("Error: Invalid merge filename format '%s'. Expected "
             "merge_{K}_{N}.plot\n",
             basename);
      return;
    }
    data_filesize = filesize - (num_files * sizeof(PlotData));
    memset(local_key, 0, 32);
    memset(local_plot_id, 0, 32);
  } else {
    char *dash = strchr(basename, '-');
    if (dash == NULL || dash - basename < 2) {
      printf(
          "Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
          basename);
      return;
    }

    if (sscanf(basename, "k%d", &k_value) != 1) {
      printf("Error: Could not parse K value from filename '%s'\n", basename);
      return;
    }

    const char *hex_start = dash + 1;
    strncpy(plot_id_string, hex_start, sizeof(plot_id_string) - 1);
    plot_id_string[64] = '\0';

    char *dot = strchr(plot_id_string, '.');
    if (dot != NULL) {
      *dot = '\0';
    }

    if (hex_string_to_byte_array(plot_id_string, local_plot_id, 32) != 0) {
      printf(
          "Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 "
          "chars). Got '%s'\n",
          basename, plot_id_string);
      return;
    }
  }

  

  if (sizeof(MemoTable2Record) == 0) {
    fprintf(stderr, "Error: sizeof(MemoTable2Record) is 0\n");
    return;
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      data_filesize / num_buckets_search / sizeof(MemoTable2Record);

  if (num_records_in_bucket_search == 0) {
    fprintf(stderr, "Error: File too small or incorrect format. Calculated 0 "
                    "records per bucket.\n");
    fprintf(stderr, "  File size: %ld, Data size: %ld\n", filesize,
            data_filesize);
    fprintf(stderr, "  sizeof(MemoTable2Record): %zu\n",
            sizeof(MemoTable2Record));
    return;
  }

  if ((unsigned long long)num_buckets_to_print > num_buckets_search) {
    num_buckets_to_print = (int)num_buckets_search;
  }

  file = fopen(filename, "rb");
  if (file == NULL) {
    printf("Error opening file %s\n", filename);
    perror("Error opening file");
    return;
  }

  PlotData *plotData_array = NULL;
  int num_files = 0;
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files_from_name;
    if (sscanf(basename, "merge_%*d_%d.plot", &num_files_from_name) == 1) {
      num_files = num_files_from_name;
      plotData_array = (PlotData *)malloc(num_files * sizeof(PlotData));
      if (plotData_array != NULL) {
        if (fseek(file, -(num_files * sizeof(PlotData)), SEEK_END) == 0) {
          size_t read_count =
              fread(plotData_array, sizeof(PlotData), num_files, file);
          if (read_count != (size_t)num_files) {
            fprintf(stderr, "Warning: Failed to read metadata footer\n");
            free(plotData_array);
            plotData_array = NULL;
            num_files = 0;
          }
        }
      }
    }
  }

  int records_per_file = (num_files > 0)
                             ? (num_records_in_bucket_search / num_files)
                             : num_records_in_bucket_search;

  if (num_files > 0 && records_per_file == 0) {
    fprintf(
        stderr,
        "Error: Too many files merged or records per bucket is too small.\n");
    fprintf(stderr, "  Merged files: %d, Records per bucket: %llu\n", num_files,
            num_records_in_bucket_search);
    fclose(file);
    if (plotData_array != NULL) {
      free(plotData_array);
    }
    return;
  }

  MemoTable2Record *buffer = (MemoTable2Record *)malloc(
      num_records_in_bucket_search * sizeof(MemoTable2Record));
  if (buffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory.\n");
    fclose(file);
    if (plotData_array != NULL) {
      free(plotData_array);
    }
    return;
  }

  printf("\n=== Printing first %d buckets from %s ===\n", num_buckets_to_print,
         basename);
  printf("File size: %ld bytes\n", filesize);
  printf("Data size: %ld bytes\n", data_filesize);
  printf("K value: %d\n", k_value);
  printf("Records per bucket: %llu\n", num_records_in_bucket_search);
  if (num_files > 0) {
    printf("Merged files: %d\n", num_files);
    printf("Records per file per bucket: %d\n", records_per_file);
  }
  printf("\n");

  for (int bucket_idx = 0; bucket_idx < num_buckets_to_print; bucket_idx++) {
    off_t offset = bucket_idx * (off_t)num_records_in_bucket_search *
                   (off_t)sizeof(MemoTable2Record);

    if (fseek(file, offset, SEEK_SET) != 0) {
      perror("Error seeking in file");
      break;
    }

    size_t records_read = fread(buffer, sizeof(MemoTable2Record),
                                num_records_in_bucket_search, file);

    uint8_t expected_prefix[PREFIX_SIZE];
    for (int i = 0; i < PREFIX_SIZE; i++) {
      expected_prefix[i] = (bucket_idx >> ((PREFIX_SIZE - 1 - i) * 8)) & 0xFF;
    }

    printf("Bucket %d (Expected prefix: ", bucket_idx);
    for (int i = 0; i < PREFIX_SIZE; i++) {
      printf("%02X ", expected_prefix[i]);
    }
    printf(") - %zu allocated slots:\n", records_read);

    int non_zero_count = 0;
    int records_shown = 0;
    const int MAX_RECORDS_TO_SHOW = 10;

    for (size_t i = 0; i < records_read; ++i) {
      if (is_record_empty(&buffer[i])) {
        if (plotData_array == NULL || num_files == 0 || records_per_file == 0) {
          break;
        }
        int current_file_index = (int)(i / records_per_file);
        size_t next_file_start =
            (size_t)((current_file_index + 1) * records_per_file);
        if (next_file_start >= records_read) {
          break;
        }
        i = next_file_start - 1;
        continue;
      }

      non_zero_count++;
      if (records_shown < MAX_RECORDS_TO_SHOW) {
        uint8_t hash_output[HASH_SIZE];

        uint8_t *record_key = local_key;
        if (plotData_array != NULL && num_files > 0 && records_per_file > 0) {
          int file_index = (int)(i / records_per_file);
          if (file_index < num_files) {
            record_key = plotData_array[file_index].key;
          }
        }

        generateBlake3Pair(buffer[i].nonce1, buffer[i].nonce2, record_key,
                           hash_output);

        printf("  [%2zu] Hash: ", i);
        for (size_t n = 0; n < HASH_SIZE; ++n) {
          printf("%02X ", hash_output[n]);
        }
        printf("| Nonce1: ");
        for (size_t n = 0; n < NONCE_SIZE; ++n) {
          printf("%02X", buffer[i].nonce1[n]);
        }
        printf(" | Nonce2: ");
        for (size_t n = 0; n < NONCE_SIZE; ++n) {
          printf("%02X", buffer[i].nonce2[n]);
        }

        bool prefix_matches = true;
        for (int p = 0; p < PREFIX_SIZE; p++) {
          if (hash_output[p] != expected_prefix[p]) {
            prefix_matches = false;
            break;
          }
        }
        printf(" %s\n", prefix_matches ? "[OK]" : "[MISMATCH]");

        records_shown++;
      }
    }

    if (non_zero_count > MAX_RECORDS_TO_SHOW) {
      printf("  ... (%d more non-zero records not shown)\n",
             non_zero_count - MAX_RECORDS_TO_SHOW);
    }
    printf("  Total: %d non-zero records out of %zu allocated slots (%.1f%% "
           "full)\n",
           non_zero_count, records_read,
           (records_read > 0) ? (100.0 * non_zero_count / records_read) : 0.0);
    printf("\n");
  }

  free(buffer);
  if (plotData_array != NULL) {
    free(plotData_array);
  }
  fclose(file);

  printf("=== End of bucket print ===\n\n");
}

// Print the first N non-empty records from a plot file. Each printed record
// shows the two nonce values (HEX) and the first 12 bytes of the Blake3 hash
// (HEX). Uses the file-unique 32-byte plot id as the Blake3 key when available
// (non-merged plots). For merged plots the derived keys from the footer are
// used as a fallback.
void print_records(const char *filename, int num_records_to_print) {
  const char *basename = strrchr(filename, '/');
  if (basename == NULL) {
    basename = filename;
  } else {
    basename++;
  }

  printf("PRINT_RECORDS: filename='%s' count=%d\n", filename,
         num_records_to_print);

  long filesize = get_file_size(filename);
  if (filesize <= 0) {
    fprintf(stderr, "Error: Invalid file size %ld for file '%s'\n", filesize,
            filename);
    return;
  }

  long data_filesize = filesize;
  uint8_t local_plot_id[32];
  uint8_t local_key[32];
  memset(local_plot_id, 0, sizeof(local_plot_id));
  memset(local_key, 0, sizeof(local_key));

  int k_value = 0;
  int num_files = 0;
  PlotData *plotData_array = NULL;

  if (strncmp(basename, "merge_", 6) == 0) {
    if (sscanf(basename, "merge_%d_%d.plot", &k_value, &num_files) != 2) {
      fprintf(stderr,
              "Error: Invalid merge filename format '%s'. Expected merge_{K}_{N}.plot\n",
              basename);
      return;
    }
    data_filesize = filesize - (num_files * sizeof(PlotData));
    // read footer keys (derived keys) for merged plots
    plotData_array = (PlotData *)malloc(num_files * sizeof(PlotData));
    if (plotData_array != NULL) {
      FILE *f = fopen(filename, "rb");
      if (f && fseek(f, -(num_files * sizeof(PlotData)), SEEK_END) == 0) {
        size_t rc = fread(plotData_array, sizeof(PlotData), num_files, f);
        if (rc != (size_t)num_files) {
          fprintf(stderr, "Warning: Failed to read metadata footer for '%s'\n",
                  filename);
          free(plotData_array);
          plotData_array = NULL;
          num_files = 0;
        }
      } else {
        if (f)
          fclose(f);
        free(plotData_array);
        plotData_array = NULL;
        num_files = 0;
      }
      if (f)
        fclose(f);
    }
  } else {
    char *dash = strchr(basename, '-');
    if (dash == NULL || dash - basename < 2) {
      fprintf(stderr,
              "Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
              basename);
      return;
    }
    if (sscanf(basename, "k%d", &k_value) != 1) {
      fprintf(stderr, "Error: Could not parse K value from filename '%s'\n",
              basename);
      return;
    }

    const char *hex_start = dash + 1;
    char plot_id_string[65];
    strncpy(plot_id_string, hex_start, sizeof(plot_id_string) - 1);
    plot_id_string[64] = '\0';
    char *dot = strchr(plot_id_string, '.');
    if (dot != NULL)
      *dot = '\0';

    if (hex_string_to_byte_array(plot_id_string, local_plot_id, 32) != 0) {
      fprintf(stderr,
              "Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 chars). Got '%s'\n",
              basename, plot_id_string);
      return;
    }
  }
  // For non-merged plots derive the per-file key the same way table2 does
  // (derive_key(k_value, plot_id, key_out)). For merged plots the footer
  // already contains derived keys in plotData_array.
  if (plotData_array == NULL || num_files == 0) {
    derive_key(k_value, local_plot_id, local_key);
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      data_filesize / num_buckets_search / sizeof(MemoTable2Record);
  if (num_records_in_bucket_search == 0) {
    fprintf(stderr,
            "Error: File too small or incorrect format. Calculated 0 records per bucket.\n");
    if (plotData_array)
      free(plotData_array);
    return;
  }

  int records_per_file = (num_files > 0)
                             ? (int)(num_records_in_bucket_search /
                                     (unsigned long long)num_files)
                             : (int)num_records_in_bucket_search;
  if (num_files > 0 && records_per_file == 0) {
    fprintf(stderr,
            "Error: Too many merged files or records per bucket too small.\n");
    if (plotData_array)
      free(plotData_array);
    return;
  }

  // open file and scan records sequentially from the start of data region
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    perror("Error opening file");
    if (plotData_array)
      free(plotData_array);
    return;
  }

  long max_offset = data_filesize;
  if (max_offset < 0) {
    fclose(file);
    if (plotData_array)
      free(plotData_array);
    return;
  }

  int printed = 0;
  MemoTable2Record rec;
  while (printed < num_records_to_print) {
    long cur_pos = ftell(file);
    if (cur_pos < 0 || cur_pos + (long)sizeof(MemoTable2Record) > max_offset)
      break; // reached end of data region

    size_t rc = fread(&rec, sizeof(MemoTable2Record), 1, file);
    if (rc != 1) {
      break; // EOF or error
    }

    if (is_record_empty(&rec)) {
      continue;
    }

    // choose key: prefer file-unique plot id when available (non-merged)
    uint8_t *key_to_use = NULL;
    uint8_t temp_key[32];
    if (plotData_array == NULL || num_files == 0) {
      // Use the derived key (SHA-256(plot_id || K)) to match table2
      // generation behavior.
      key_to_use = local_key;
    } else {
      // For merged plots the footer stores derived keys; select the
      // appropriate derived key for this record based on records_per_file.
      unsigned long long slot_index =
          (unsigned long long)(cur_pos / (long)sizeof(MemoTable2Record));
      unsigned long long bucket_slot =
          slot_index % num_records_in_bucket_search; // reset per-bucket

      int file_index = 0;
      if (records_per_file > 0) {
        file_index = (int)(bucket_slot /
                           (unsigned long long)records_per_file);
        if (file_index >= num_files)
          file_index = num_files - 1;
      }
      if (plotData_array != NULL && file_index >= 0 && file_index < num_files) {
        key_to_use = plotData_array[file_index].key;
      } else {
        // fallback to local plot id if something odd happened
        memcpy(temp_key, local_plot_id, 32);
        key_to_use = temp_key;
      }
    }

    uint8_t hash_output[HASH_SIZE];
    generateBlake3Pair(rec.nonce1, rec.nonce2, key_to_use, hash_output);

    // print first 12 bytes of hash, then ' <= ', then NONCE1,NONCE2
    for (int h = 0; h < 12 && h < HASH_SIZE; ++h)
      printf("%02X", hash_output[h]);
    printf(" <= ");
    for (size_t n = 0; n < NONCE_SIZE; ++n)
      printf("%02X", rec.nonce1[n]);
    printf(",");
    for (size_t n = 0; n < NONCE_SIZE; ++n)
      printf("%02X", rec.nonce2[n]);
    printf("\n");

    printed++;
  }

  if (plotData_array)
    free(plotData_array);
  fclose(file);

  if (printed == 0) {
    printf("No non-empty records found in '%s'\n", filename);
  }
}
