#include "search.h"

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
  if (strncmp(basename, "merge_", 6) == 0) {
    int num_files_from_name;
    if (sscanf(basename, "merge_%*d_%d.plot", &num_files_from_name) == 1) {
      ctx->num_files = num_files_from_name;
      ctx->plotData_array =
          (PlotData *)malloc(ctx->num_files * sizeof(PlotData));
      if (ctx->plotData_array != NULL) {
        if (fseek(ctx->file, -(ctx->num_files * sizeof(PlotData)), SEEK_END) ==
            0) {
          size_t read_count = fread(ctx->plotData_array, sizeof(PlotData),
                                    ctx->num_files, ctx->file);
          if (read_count != (size_t)ctx->num_files) {
            fprintf(stderr, "Warning: Failed to read metadata footer\n");
            free(ctx->plotData_array);
            ctx->plotData_array = NULL;
            ctx->num_files = 0;
          }
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

MemoTable2Record *search_memo_record(
    FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH,
    unsigned long long num_records_in_bucket_search, MemoTable2Record *buffer,
    int num_threads_bucket, PlotData *plotData, int total_files,
    int records_per_file, uint8_t *default_key) {
  size_t records_read;
  MemoTable2Record *foundRecord = NULL;

  // Define the offset you want to seek to
  off_t offset = bucketIndex * (off_t)num_records_in_bucket_search *
                 (off_t)sizeof(MemoTable2Record);
  if (DEBUG)
    printf("SEARCH: seek to %" PRIuMAX " offset\n", (uintmax_t)offset);

  // Seek to the specified offset
  if (fseek(file, offset, SEEK_SET) != 0) {
    perror("Error seeking in file");
    return NULL;
  }

  records_read = fread(buffer, sizeof(MemoTable2Record),
                       num_records_in_bucket_search, file);
  if (records_read > 0) {
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

    int found = 0;
    size_t records_checked = 0;
    size_t first_match_index = (size_t)-1;

    size_t effective_records_read = records_read;
    if (plotData == NULL || total_files == 0 || records_per_file == 0) {
      for (size_t i = 0; i < records_read; ++i) {
        if (is_record_empty(&buffer[i])) {
          effective_records_read = i;
          break;
        }
      }
    }

#pragma omp parallel shared(found) num_threads(num_threads_bucket)
    {
#pragma omp for
      for (size_t i = 0; i < effective_records_read; ++i) {
        if (plotData != NULL && total_files > 0 && records_per_file > 0) {
          if (is_record_empty(&buffer[i])) {
            continue;
          }
        }

        if (!found) {
          uint8_t hash_output[HASH_SIZE];

          // Determine which key to use based on record position
          uint8_t *record_key =
              default_key; // Default to passed-in key for non-merged files
          if (plotData != NULL && total_files > 0 && records_per_file > 0) {
            int file_index = (int)(i / records_per_file);
            if (file_index < total_files) {
              record_key = plotData[file_index].key;
            }
          }

          generateBlake3Pair(buffer[i].nonce1, buffer[i].nonce2, record_key,
                             hash_output);

#pragma omp atomic
          records_checked++;

          if (DEBUG) {
#pragma omp critical
            {
              int is_match =
                  (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0);
              printf("[%s] Query: ", is_match ? "MATCH" : "MISS ");
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

          // Compare the first PREFIX_SIZE bytes of the current hash to the
          // previous hash prefix
          if (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0) {
#pragma omp critical
            {
              if (first_match_index == (size_t)-1) {
                first_match_index = i;
              }
            }

#pragma omp atomic write
            found = 1;
          } else {
            //++count_condition_not_met;

            /*
                                if (!DEBUG)
                                {
                                // Print previous hash and nonce, and current
               hash and nonce
                                //printf("Condition not met at record %zu:\n",
               total_records);
                                //printf("Search string %s\n",SEARCH_STRING);

                                printf("Search hash prefix (UINT8): ");
                                for (size_t n = 0; n < PREFIX_SIZE; ++n)
                                    printf("%02X", SEARCH_UINT8[n]);
                                printf("\n");

                                printf("Current nonce: ");
                                for (size_t n = 0; n < NONCE_SIZE; ++n)
                                    printf("%02X", buffer[i].nonce[n]);
                                printf("\n");
                                printf("Current hash prefix: ");
                                for (size_t n = 0; n < HASH_SIZE_SEARCH; ++n)
                                    printf("%02X", hash_output[n]);
                                printf("\n");
                                }
                                */
          }
        }
      }
    }

    if (first_match_index != (size_t)-1) {
      foundRecord = &buffer[first_match_index];
    }

    if (DEBUG) {
      printf("\nBucket scan complete: %zu records checked, %s\n",
             records_checked, found ? "MATCH FOUND" : "NO MATCH");
      printf("===================\n\n");
    }
  } else {
    printf("error reading from file..\n");
  }
  return foundRecord;
}

// not sure if the search of more than PREFIX_LENGTH works
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
  bool foundRecord = false;
  MemoTable2Record *fRecord = NULL;

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
  fRecord = search_memo_record(file, bucketIndex, SEARCH_UINT8, SEARCH_LENGTH,
                               num_records_in_bucket_search, buffer,
                               num_threads_bucket, plotData_array, num_files,
                               records_per_file, local_key);
  if (fRecord != NULL)
    foundRecord = true;
  else
    foundRecord = false;

  // Clean up
  if (plotData_array != NULL) {
    free(plotData_array);
  }

  double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

  // Check for reading errors
  if (ferror(file)) {
    perror("Error reading file");
  }

  // Clean up
  fclose(file);
  free(buffer);

  result.search_time_ms = elapsed_time;
  result.avg_time_per_lookup_ms = elapsed_time;
  if (foundRecord) {
    result.found_count = 1;
    result.not_found_count = 0;
  } else {
    result.found_count = 0;
    result.not_found_count = 1;
  }

  // Print the total number of times the condition was met
  if (foundRecord == true) {
    printf("NONCE found (");
    for (size_t n = 0; n < NONCE_SIZE; ++n)
      printf("%02X", fRecord->nonce1[n]);
    printf(", ");
    for (size_t n = 0; n < NONCE_SIZE; ++n)
      printf("%02X", fRecord->nonce2[n]);
    printf(") for HASH prefix %s\n", SEARCH_STRING);
  } else
    printf("no NONCE found for HASH prefix %s\n", SEARCH_STRING);
  printf("search time %.2f ms\n", elapsed_time);

  return result;
}

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
  MemoTable2Record *fRecord = NULL;

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
    fRecord = search_memo_record(
        file, getBucketIndex(SEARCH_UINT8), SEARCH_UINT8, SEARCH_LENGTH,
        num_records_in_bucket_search, buffer, num_threads_bucket,
        plotData_array, num_files, records_per_file, local_key);
    // }

    if (fRecord != NULL)
      foundRecords++;
    else
      notFoundRecords++;

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

  off_t bucketIndex = getBucketIndex(query);

  double start_time = omp_get_wtime();

  MemoTable2Record *fRecord = search_memo_record(
      ctx->file, bucketIndex, (uint8_t *)query, search_length,
      ctx->num_records_in_bucket_search, ctx->buffer, num_threads_bucket,
      ctx->plotData_array, ctx->num_files, ctx->records_per_file,
      ctx->local_key);

  double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;
  result.search_time_ms = elapsed_time;
  result.avg_time_per_lookup_ms = elapsed_time;
  if (fRecord != NULL) {
    result.found_count = 1;
    result.not_found_count = 0;
  } else {
    result.found_count = 0;
    result.not_found_count = 1;
  }

  return result;
}

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

  derive_key(k_value, local_plot_id, local_key);

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
