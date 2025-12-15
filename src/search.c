#include "search.h"

MemoTable2Record *
search_memo_record(FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8,
                   size_t SEARCH_LENGTH,
                   unsigned long long num_records_in_bucket_search,
                   MemoTable2Record *buffer, int num_threads_bucket) {
  const int HASH_SIZE_SEARCH = 8;
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
    fclose(file);
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

    int found = 0; // Shared flag to indicate termination
    size_t records_checked = 0;

#pragma omp parallel shared(found) num_threads(num_threads_bucket)
    {
#pragma omp for
      for (size_t i = 0; i < records_read; ++i) {
#pragma omp cancellation point for
        if (!found && is_nonce_nonzero(buffer[i].nonce1, NONCE_SIZE) &&
            is_nonce_nonzero(buffer[i].nonce2, NONCE_SIZE)) {
          uint8_t hash_output[HASH_SIZE_SEARCH];

          generateBlake3Pair(buffer[i].nonce1, buffer[i].nonce2, key,
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
            foundRecord = &buffer[i];

#pragma omp atomic write
            found = 1;

#pragma omp cancel for
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
  uint8_t SEARCH_UINT8[HASH_SIZE];
  size_t SEARCH_LENGTH = strlen(SEARCH_STRING) / 2;

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

  // Extract K value and hex plot ID from filename format: k{K}-{hex_id}.plot
  int k_value;
  char plot_id_string[65];
  char *dash = strchr(basename, '-');
  if (dash == NULL || dash - basename < 2) {
    printf("Error: Invalid filename format '%s'. Expected k{K}-{hex_id}.plot\n",
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

  if (hex_string_to_byte_array(plot_id_string, plot_id, 32) != 0) {
    printf("Error: Invalid plot ID in filename '%s'. Expected 32 bytes hex (64 "
           "chars). Got '%s'\n",
           basename, plot_id_string);
    return result;
  }

  derive_key(k_value, plot_id, key);

  if (filesize != -1) {
    result.filesize = filesize;
    if (!BENCHMARK)
      printf("Size of '%s' is %ld bytes.\n", filename, filesize);
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      filesize / num_buckets_search / sizeof(MemoRecord);
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

  fRecord = search_memo_record(file, bucketIndex, SEARCH_UINT8, SEARCH_LENGTH,
                               num_records_in_bucket_search, buffer,
                               num_threads_bucket);
  if (fRecord != NULL)
    foundRecord = true;
  else
    foundRecord = false;

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

  srand((unsigned int)time(NULL));

  size_t SEARCH_LENGTH = (difficulty == 0) ? HASH_SIZE : difficulty;
  MemoTable2Record *buffer = NULL;

  FILE *file = NULL;
  int foundRecords = 0;
  int notFoundRecords = 0;
  MemoTable2Record *fRecord = NULL;

  long filesize = get_file_size(filename);
  result.filesize = filesize;

  if (filesize != -1) {
    if (!BENCHMARK)
      printf("Size of '%s' is %ld bytes.\n", filename, filesize);
  }

  unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
  unsigned long long num_records_in_bucket_search =
      filesize / num_buckets_search / sizeof(MemoTable2Record);
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

  double start_time = omp_get_wtime();

  uint8_t SEARCH_UINT8[SEARCH_LENGTH];

  for (int i = 0; i < num_lookups; i++) {
    for (int j = 0; j < SEARCH_LENGTH; ++j) {
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
    fRecord = search_memo_record(
        file, getBucketIndex(SEARCH_UINT8), SEARCH_UINT8, SEARCH_LENGTH,
        num_records_in_bucket_search, buffer, num_threads_bucket);
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
