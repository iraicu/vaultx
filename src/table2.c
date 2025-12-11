#include "table2.h"
#include <stdio.h>

// Wrapper function to generate hash for two nonces using crypto's
// generateBlake3Pair
void generate_hash2(uint8_t *nonce1, uint8_t *nonce2, uint8_t *hash) {
  generateBlake3Pair(nonce1, nonce2, key, hash);
}

// In-memory merge approach: Find matches between nonces in buckets
void findMatches() {

  // FIXME: How to use a good bucket distance
  // Ideas: Average distance, multiply expected distance by a constant
  uint64_t expected_distance = 1ULL << (64 - K);

#pragma omp parallel for schedule(static)
  for (unsigned long long b = 0; b < total_buckets; b += BATCH_SIZE) {

    unsigned long long end = b + BATCH_SIZE;
    if (end > total_buckets) {
      end = total_buckets;
    }

    for (unsigned long long k = b; k <= end; k++) {
      int n = buckets[k].count;
      if (n <= 1) {
        continue;
      }

      // Allocate temporary sort array
      MemoRecordWithHash *sortArray = malloc(n * sizeof(MemoRecordWithHash));
      if (!sortArray) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
      }

      // Precompute hashes
      for (int i = 0; i < n; i++) {
        sortArray[i].record = &buckets[k].records[i];
        generateBlake3(sortArray[i].record->nonce, key, sortArray[i].hash);
      }

      // Sort by precomputed hashes
      qsort(sortArray, n, sizeof(MemoRecordWithHash), compare_hash_wrapper);

      // Pairwise distance checking on sorted hashes
      for (int i = 0; i + 1 < n; i++) {
        uint8_t *hash1 = sortArray[i].hash;
        int j = i + 1;

        while (j < n) {
          uint8_t *hash2 = sortArray[j].hash;

          uint64_t distance = compute_hash_distance(hash1, hash2, HASH_SIZE);
          if (distance > expected_distance) {
            break;
          }

          MemoTable2Record record;
          memcpy(record.nonce1, sortArray[i].record->nonce, NONCE_SIZE);
          memcpy(record.nonce2, sortArray[j].record->nonce, NONCE_SIZE);

          uint8_t hash[HASH_SIZE];
          generateBlake3Pair(record.nonce1, record.nonce2, key, hash);

          size_t bucketIndex = getBucketIndex(hash);
          insert_record2_table(table2, buckets_table2, &record, bucketIndex);

          j++;
        }
      }

      // Clear bucket
      free(sortArray);
      memset(buckets[k].records, 0, sizeof(MemoRecord) * num_records_in_bucket);
    }
  }
}

// In-memory approach: Insert record into flat table2 array
int insert_record2_table(MemoTable2Record *table2, BucketTable2 *buckets2,
                         MemoTable2Record *record, size_t bucketIndex) {
  if (bucketIndex >= total_buckets) {
    fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n",
            bucketIndex, total_buckets - 1);
    return 0;
  }

  BucketTable2 *bucket = &buckets2[bucketIndex];
  size_t idx;

  // Atomically capture the current count and increment it
#pragma omp atomic capture
  {
    idx = bucket->count;
    bucket->count++;
  }

  // Check if there's room in the bucket
  if (idx < num_records_in_bucket) {
    memcpy(table2[bucketIndex * num_records_in_bucket + idx].nonce1,
           record->nonce1, NONCE_SIZE);
    memcpy(table2[bucketIndex * num_records_in_bucket + idx].nonce2,
           record->nonce2, NONCE_SIZE);
    return 1;
  } else {
    // Ensure count doesn't exceed the maximum allowed
    bucket->count = num_records_in_bucket;
    if (!bucket->full) {
#pragma omp atomic
      full_buckets_global++;
      bucket->full = true;
    }
    bucket->count_waste++;
    // Overflow handling can be added here if necessary.
    return 0;
  }
}

// Out-of-memory approach: Insert record into buckets2 structure
void insert_record2_buckets(BucketTable2 *buckets2, MemoTable2Record *record,
                            size_t bucketIndex) {
  if (bucketIndex >= total_buckets) {
    fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n",
            bucketIndex, total_buckets - 1);
    return;
  }

  BucketTable2 *bucket = &buckets2[bucketIndex];
  size_t idx;

  // Atomically capture the current count and increment it
#pragma omp atomic capture
  {
    idx = bucket->count;
    bucket->count++;
  }

  // Check if there's room in the bucket
  if (idx < num_records_in_bucket) {
    memcpy(bucket->records[idx].nonce1, record->nonce1, NONCE_SIZE);
    memcpy(bucket->records[idx].nonce2, record->nonce2, NONCE_SIZE);
  } else {
    // Ensure count doesn't exceed the maximum allowed
    bucket->count = num_records_in_bucket;
    if (!bucket->full) {
#pragma omp atomic
      full_buckets_global++;
      bucket->full = true;
    }
    bucket->count_waste++;
    // Overflow handling can be added here if necessary.
  }
}

uint64_t compute_hash_distance(const uint8_t *hash_output,
                               const uint8_t *prev_hash, size_t hash_size) {
  // Ensure there are at least 8 bytes in the hash
  if (hash_size < 8) {
    fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
    return 0;
  }

  // Convert the first 8 bytes of each hash to a uint64_t
  uint64_t current = byteArrayToLongLong(hash_output, 8);
  uint64_t previous = byteArrayToLongLong(prev_hash, 8);

  // Return absolute difference
  return current > previous ? current - previous : previous - current;
}

// Write table2 to disk in chunks
void writeTable2(uint8_t *plot_id) {
  char FILENAME[256];

  char filename[128];
  snprintf(filename, sizeof(filename), "K%d_%s.plot", K,
           byteArrayToHexString(plot_id, 32));
  path_join(FILENAME, sizeof(FILENAME), DESTINATION, filename);

  int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  if (fd == -1) {
    printf("Error opening file %s (#4)\n", FILENAME);
    perror("Error opening file");
    return;
  }

  size_t total_size = total_nonces * sizeof(MemoTable2Record);
  char *data_ptr = (char *)table2;
  size_t total_written = 0;
  const size_t chunk_size = 4 * 1024 * 1024;

  while (total_written < total_size) {
    size_t remaining = total_size - total_written;
    size_t to_write = remaining < chunk_size ? remaining : chunk_size;

    ssize_t bytes = write(fd, data_ptr + total_written, to_write);

    if (bytes < 0) {
      fprintf(stderr, "Error writing to file at offset %zu: %s\n",
              total_written, strerror(errno));
      close(fd);
      return;
    }

    total_written += bytes;
  }

  close(fd);
}

int print_table2_entry(uint8_t *nonce_output, uint8_t *prev_nonce,
                       const uint8_t *hash_output, const uint8_t *prev_hash,
                       size_t hash_size) {
  // Ensure there are at least 8 bytes in the hash
  if (hash_size < 8) {
    fprintf(stderr, "Error: hash_size must be at least 8 bytes.\n");
    return -1;
  }

  // Print debugging information
  fprintf(stderr, "Debug Info:\n");

  // Print the first 8 bytes of hash_output
  fprintf(stderr, "hash_output (first 8 bytes): ");
  for (size_t i = 0; i < 8; i++) {
    fprintf(stderr, "%02X ", hash_output[i]);
  }
  fprintf(stderr, "\n");

  // Print the first 8 bytes of prev_hash
  fprintf(stderr, "prev_hash (first 8 bytes): ");
  for (size_t i = 0; i < 8; i++) {
    fprintf(stderr, "%02X ", prev_hash[i]);
  }
  fprintf(stderr, "\n");

  uint8_t hash_table2[hash_size];
  generate_hash2(prev_nonce, nonce_output, hash_table2);

  // Print the first 8 bytes of hash_table2
  fprintf(stderr, "hash_table2 (first 8 bytes): ");
  for (size_t i = 0; i < 8; i++) {
    fprintf(stderr, "%02X ", hash_table2[i]);
  }
  fprintf(stderr, "\n");

  return 0;
}

size_t process_memo_records_table2(const char *filename,
                                   const size_t BATCH_SIZE) {
  // --- open file & figure out how many records are in it ---
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("Error opening file");
    return 0;
  }
  fseek(file, 0, SEEK_END);
  long filesize = ftell(file);
  if (filesize < 0) {
    perror("ftell failed");
    fclose(file);
    return 0;
  }
  size_t total_recs_in_file = filesize / sizeof(MemoTable2Record);
  size_t num_buckets = (total_recs_in_file + BATCH_SIZE - 1) / BATCH_SIZE;
  rewind(file);

  // --- allocate one batch buffer ---
  MemoTable2Record *buffer = malloc(BATCH_SIZE * sizeof(MemoTable2Record));
  if (!buffer) {
    fprintf(stderr, "Error: Unable to allocate buffer for %zu records\n",
            BATCH_SIZE);
    fclose(file);
    return 0;
  }

  // --- state & counters ---
  size_t total_records = 0;
  size_t zero_nonce_count = 0;
  size_t full_buckets = 0;
  size_t count_condition_met = 0;
  size_t count_condition_not_met = 0;

  uint8_t prev_hash[HASH_SIZE] = {0};
  uint8_t prev_nonce1[NONCE_SIZE] = {0};
  uint8_t prev_nonce2[NONCE_SIZE] = {0};

  // --- timing for progress updates ---
  double start_time = omp_get_wtime();
  double last_print_time = start_time;

  // --- read & process in one pass, printing progress every second ---
  for (size_t bucket = 0; bucket < num_buckets; bucket++) {
    bool bucket_not_full = false;
    size_t records_read =
        fread(buffer, sizeof(MemoTable2Record), BATCH_SIZE, file);
    if (records_read == 0)
      break;

    for (size_t i = 0; i < records_read; i++) {
      ++total_records;

      if (is_nonce_nonzero(buffer[i].nonce1, NONCE_SIZE) &&
          is_nonce_nonzero(buffer[i].nonce2, NONCE_SIZE)) {
        // compute the hash
        uint8_t hash_output[HASH_SIZE];
        generate_hash2(buffer[i].nonce1, buffer[i].nonce2, hash_output);

        // compare prefix to previous
        if (memcmp(hash_output, prev_hash, PREFIX_SIZE) >= 0) {
          ++count_condition_met;
        } else {
          ++count_condition_not_met;
          if (DEBUG) {
            // your debug prints...
          }
        }

        // update previous
        memcpy(prev_hash, hash_output, HASH_SIZE);
        memcpy(prev_nonce1, buffer[i].nonce1, NONCE_SIZE);
        memcpy(prev_nonce2, buffer[i].nonce2, NONCE_SIZE);
      } else {
        ++zero_nonce_count;
        bucket_not_full = true;
      }

      // --- progress update every second ---
      double now = omp_get_wtime();
      if (!BENCHMARK && (now - last_print_time >= 1.0)) {
        last_print_time = now;
        double elapsed = now - start_time;
        double pct = (double)total_records * 100.0 / (double)total_recs_in_file;
        double pct_met = (double)count_condition_met * 100.0 /
                         (double)(count_condition_met +
                                  count_condition_not_met + zero_nonce_count);
        double pct_sorted =
            (double)count_condition_met * 100.0 /
            (double)(count_condition_met + count_condition_not_met);
        printf(
            "[%.2f] Verify %.2f%%: Sorted %.2f%% : Storage Efficiency %.2f%%\n",
            elapsed, pct, pct_sorted, pct_met);
        printf("Zero Nonces: %zu, Condition Met: %zu, Not Met: %zu, Total "
               "Records: %zu\n",
               zero_nonce_count, count_condition_met, count_condition_not_met,
               total_records);
      }
    }

    if (!bucket_not_full) {
      ++full_buckets;
    }
  }

  // ensure final 100% progress line
  double now = omp_get_wtime();
  double elapsed = now - start_time;
  double pct = (double)total_records * 100.0 / (double)total_recs_in_file;
  double pct_met = (double)count_condition_met * 100.0 /
                   (double)(count_condition_met + count_condition_not_met +
                            zero_nonce_count);
  double pct_sorted = (double)count_condition_met * 100.0 /
                      (double)(count_condition_met + count_condition_not_met);

  if (!BENCHMARK) {
    printf("[%.2f] Verify %.2f%%: Sorted %.2f%% : Storage Efficiency %.2f%%\n",
           elapsed, pct, pct_sorted, pct_met);
  } else {
    printf("%.2f%%\n", pct_met);
  }
  // --- cleanup ---
  free(buffer);
  fclose(file);

  return count_condition_met;
}

unsigned long long generate_table2(MemoRecord *sorted_records,
                                   size_t num_records_in_bucket) {
  uint64_t expected_distance = (1ULL << (64 - K)) * (1 / matching_factor);

  unsigned long long match_counter_per_bucket = 0;

  // num_records_in_bucket = num_records_in_shuffled_bucket =
  // num_records_in_bucket * rounds
  for (size_t i = 0; i < num_records_in_bucket; ++i) {
    if (is_nonce_nonzero(sorted_records[i].nonce, NONCE_SIZE)) {
      // Compute Blake3 hash for record i
      uint8_t hash_i[HASH_SIZE];
      generate_hash(sorted_records[i].nonce, hash_i);

      // Compare hash_i with all subsequent non-zero nonce records
      for (size_t j = i + 1; j < num_records_in_bucket; ++j) {
        // Skip records with zero nonce
        if (!is_nonce_nonzero(sorted_records[j].nonce, NONCE_SIZE)) {
          continue;
        }

        // Compute Blake3 hash for record j
        uint8_t hash_j[HASH_SIZE];
        generate_hash(sorted_records[j].nonce, hash_j);

        // Compute the distance between hash_i and hash_j
        uint64_t distance = compute_hash_distance(hash_i, hash_j, HASH_SIZE);

        // Because data is sorted, break out of the inner loop once the distance
        // exceeds expected_distance
        if (distance > expected_distance) {
          break;
        }

        match_counter_per_bucket++;

        MemoTable2Record record;
        uint8_t hash_table2[HASH_SIZE];
        memcpy(record.nonce1, sorted_records[i].nonce, NONCE_SIZE);
        memcpy(record.nonce2, sorted_records[j].nonce, NONCE_SIZE);

        generate_hash2(record.nonce1, record.nonce2, hash_table2);

        if (MEMORY_WRITE) {
          off_t bucketIndex = getBucketIndex(hash_table2);
          insert_record2_buckets(buckets2, &record, bucketIndex);
        }
      }
    }
  }
  return match_counter_per_bucket;
}
