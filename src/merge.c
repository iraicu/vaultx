#define _GNU_SOURCE
#include "merge.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

// Print only when not in BENCHMARK mode. Use runtime check so the
// BENCHMARK global can be toggled via CLI.
#define BPRINTF(...) \
  do { if (!BENCHMARK) printf(__VA_ARGS__); } while (0)

#include <sys/resource.h>

/* Real high-water RSS of the process, in MB. vaultx.c samples peak memory
   *before* merge() is called, so its figure never includes the merge buffers;
   this is sampled after the merge so the reported peak actually covers them. */
static double merge_peak_rss_mb(void) {
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) != 0) return -1.0;
  return (double)ru.ru_maxrss / 1024.0; /* ru_maxrss is KB on Linux */
}

#if defined(__linux__)
#include <sched.h>
#include <sys/syscall.h>
#define HAVE_CPU_PINNING 1
#else
#define HAVE_CPU_PINNING 0
#endif

#ifdef ENABLE_NUMA
void print_numa_node(void *ptr, const char *label) {
  int status;
  int ret = get_mempolicy(&status, NULL, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR);
  if (ret != 0) {
    perror("get_mempolicy");
    exit(EXIT_FAILURE);
  }
  BPRINTF("%s is on NUMA node %d\n", label, status);
}
#endif

int get_current_cpu() {
#if HAVE_CPU_PINNING
  return sched_getcpu(); // Returns the CPU number the calling thread is running
                         // on
#else
  return 0;
#endif
}

void pin_thread_to_cpu(int cpu_num) {
#if HAVE_CPU_PINNING
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_num, &cpuset);

  pid_t tid = syscall(SYS_gettid); // get calling thread's tid
  int ret = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
  if (ret != 0) {
    perror("sched_setaffinity");
  } else {
    // printf("Thread %d pinned to CPU %d\n", tid, cpu_num);
  }
#else
  (void)cpu_num;
#endif
}

#define batch_read(batch_idx)                                                  \
  do {                                                                         \
    if (DEBUG) {                                                               \
      printf("[%d] Read Started\n", batch_idx);                                \
    }                                                                          \
                                                                               \
    double start_time = omp_get_wtime();                                       \
    /* uint64_t batch_read_syscalls = 0; */                                    \
    /* uint64_t batch_read_bytes = 0; */                                       \
                                                                               \
    MergeBatch *mergeBatch = &mergeBatches[batch_idx];                         \
    mergeBatch->buffer = (MemoTable2Record *)calloc(                           \
        buckets_in_batch * records_per_global_bucket,                          \
        sizeof(MemoTable2Record));                                             \
                                                                               \
    if (mergeBatch->buffer == NULL) {                                          \
      fprintf(stderr,                                                          \
              "Error: Failed to allocate memory for buckets (batch %d)\n",     \
              batch_idx);                                                      \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
                                                                               \
    char *buf = (char *)mergeBatch->buffer;                                    \
                                                                               \
    _Pragma("omp parallel for num_threads(MERGE_IO_THREADS) schedule(static)") \
    for (int f = 0; f < TOTAL_FILES; f++) {                                    \
      int fd = fds[f];                                                         \
      size_t total_bytes =                                                     \
          buckets_in_batch * num_records_in_bucket * sizeof(MemoTable2Record); \
      size_t bytes_read = 0;                                                   \
      char *dest = buf + f * total_bytes;                                      \
                                                                               \
      while (bytes_read < total_bytes) {                                       \
        /* double read_syscall_start = (ENABLE_DETAILED_METRICS) ?             \
         * omp_get_wtime() : 0; */                                             \
        ssize_t res = read(fd, dest + bytes_read, total_bytes - bytes_read);   \
        /* if (ENABLE_DETAILED_METRICS) { */                                   \
        /*     global_metrics.merge.total_read_time += omp_get_wtime() -       \
         * read_syscall_start; */                                              \
        /*     batch_read_syscalls++; */                                       \
        /* } */                                                                \
        if (res < 0) {                                                         \
          perror("read failed");                                               \
          close(fd);                                                           \
          break;                                                               \
        } else if (res == 0) {                                                 \
          fprintf(stderr, "Unexpected EOF on file %d\n", f);                   \
          break;                                                               \
        }                                                                      \
        bytes_read += res;                                                     \
        /* batch_read_bytes += res; */                                         \
      }                                                                        \
                                                                               \
      if (bytes_read < total_bytes) {                                          \
        fprintf(stderr, "Only read %zu of %zu bytes from file %d\n",           \
                bytes_read, total_bytes, f);                                   \
      }                                                                        \
    }                                                                          \
                                                                               \
    double elapsed = omp_get_wtime() - start_time;                             \
    mergeBatch->total_time += elapsed;                                         \
    read_total_time += elapsed;                                                \
                                                                               \
    /* if (ENABLE_DETAILED_METRICS) { */                                       \
    /*     global_metrics.merge.read_syscalls += batch_read_syscalls; */       \
    /*     global_metrics.merge.total_bytes_read += batch_read_bytes; */       \
    /* } */                                                                    \
                                                                               \
    if (DEBUG) {                                                               \
      printf("[%d] Read Complete: %.2fs\n", batch_idx, elapsed);               \
    }                                                                          \
                                                                               \
    mergeBatch->readDone = true;                                               \
  } while (0)

#define batch_write(batch_idx)                                                 \
  do {                                                                         \
    if (DEBUG) {                                                               \
      printf("[%d] Write Started\n", batch_idx);                               \
    }                                                                          \
                                                                               \
    MergeBatch *mergeBatch = &mergeBatches[batch_idx];                         \
    double write_start_time = omp_get_wtime();                                 \
    /* uint64_t batch_write_syscalls = 0; */                                   \
    /* uint64_t batch_write_bytes = 0; */                                      \
                                                                               \
    size_t total_bytes = buckets_in_batch * records_per_global_bucket *        \
                         sizeof(MemoTable2Record);                             \
    size_t bytes_written = 0;                                                  \
    char *src = (char *)mergeBatch->mergedBuckets;                             \
                                                                               \
    while (bytes_written < total_bytes) {                                      \
      /* double write_syscall_start = (ENABLE_DETAILED_METRICS) ?              \
       * omp_get_wtime() : 0; */                                               \
      ssize_t res =                                                            \
          write(merge_fd, src + bytes_written, total_bytes - bytes_written);   \
      /* if (ENABLE_DETAILED_METRICS) { */                                     \
      /*     global_metrics.merge.total_write_time += omp_get_wtime() -        \
       * write_syscall_start; */                                               \
      /*     batch_write_syscalls++; */                                        \
      /* } */                                                                  \
      if (res < 0) {                                                           \
        perror("Error writing to merge file");                                 \
        close(merge_fd);                                                       \
        exit(EXIT_FAILURE);                                                    \
      } else if (res == 0) {                                                   \
        fprintf(stderr, "Unexpected zero write\n");                            \
        break;                                                                 \
      }                                                                        \
      bytes_written += res;                                                    \
      /* batch_write_bytes += res; */                                          \
    }                                                                          \
                                                                               \
    if (bytes_written < total_bytes) {                                         \
      fprintf(stderr, "Only wrote %zu of %zu bytes\n", bytes_written,          \
              total_bytes);                                                    \
    }                                                                          \
                                                                               \
    free(mergeBatch->mergedBuckets);                                           \
                                                                               \
    double write_time = omp_get_wtime() - write_start_time;                    \
    write_total_time += write_time;                                            \
    mergeBatch->total_time += write_time;                                      \
                                                                               \
    /* if (ENABLE_DETAILED_METRICS) { */                                       \
    /*     global_metrics.merge.write_syscalls += batch_write_syscalls; */     \
    /*     global_metrics.merge.total_bytes_written += batch_write_bytes; */   \
    /*     if (write_time > 0) { */                                            \
    /*         global_metrics.merge.write_throughput_MBps = (batch_write_bytes \
     * / 1e6) / write_time; */                                                 \
    /*     } */                                                                \
    /* } */                                                                    \
                                                                               \
    if (DEBUG) {                                                               \
      printf("[%d] Write Complete: %.2fs\n", batch_idx, write_time);           \
    }                                                                          \
                                                                               \
    double write_throughput_MBps = BATCH_MEMORY_MB / write_time;               \
    (void)write_throughput_MBps;                                               \
    double progress = ((double)end_bucket / total_buckets);                    \
    double elapsed_time = omp_get_wtime() - start_time;                        \
    printf("[%6.2f%%] | Batch %-6d (%9.2fs) | Total: %9.2fs | Time Left: "     \
           "%9.2fs\n",                                                         \
           progress * 100, batch_idx, mergeBatch->total_time, elapsed_time,    \
           elapsed_time * (1 - progress) / progress);                          \
  } while (0)

int merge() {

  int MAX_FILENAME_LEN = 256;
  char filenames[TOTAL_FILES][MAX_FILENAME_LEN];
  const char *dir_name = SOURCE;
  int desired_k = K;

  DIR *d = opendir(dir_name);
  struct dirent *dir;

  int stored = 0;
  int matching = 0;

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (dir->d_name[0] != '.' && dir->d_name[0] == 'k' &&
          isdigit(dir->d_name[1]) && strstr(dir->d_name, ".plot") != NULL) {
        int kVal = 0;
        char hex[129];
        if (sscanf(dir->d_name, "k%d-%128[^.].plot", &kVal, hex) != 2) {
          continue;
        }
        if (kVal != desired_k) {
          continue;
        }

        matching++;
        if (stored < TOTAL_FILES) {
          path_join(filenames[stored], MAX_FILENAME_LEN, dir_name, dir->d_name);
          filenames[stored][MAX_FILENAME_LEN - 1] = '\0'; // safety null-terminate
          stored++;
        }
      }
    }
    closedir(d);
  } else {
    perror("opendir");
    return 1;
  }

  if (matching < TOTAL_FILES) {
    fprintf(stderr,
            "Error: Expected %d plot(s) with K=%d but found %d in '%s'\n",
            TOTAL_FILES, desired_k, matching, dir_name);
    return 1;
  }

  PlotData plotData[TOTAL_FILES];

  for (int i = 0; i < TOTAL_FILES; i++) {
    char *filename = filenames[i];

    filename += strlen(dir_name);
    if (*filename == '/') {
      filename++;
    }

    int kVal = 0;
    char hex[129];
    if (sscanf(filename, "k%d-%128[^.].plot", &kVal, hex) != 2) {
      fprintf(stderr, "Error: Failed to parse filename '%s'\n", filename);
      fprintf(
          stderr,
          "Expected format: k{K}-{hex}.plot (e.g., k27-abc123...def.plot)\n");
      return 1;
    }

    uint8_t *plot_id = hexStringToByteArray(hex);
    if (plot_id == NULL) {
      fprintf(stderr, "Error: Failed to convert hex string '%s' to bytes\n",
              hex);
      return 1;
    }

    plotData[i].K = kVal;
    K = kVal;
    derive_key(kVal, plot_id, plotData[i].key);
    free(plot_id);
  }

  /* For merge-only, ensure bucket sizing matches the source plots rather than
   * any -m value passed on the CLI. All merged files must share the same K. */
  {
    int first_k = plotData[0].K;
    K = first_k;
    total_buckets = 1ULL << (PREFIX_SIZE * 8);
    unsigned long long num_records_total_local = 1ULL << K;
    num_records_in_bucket = num_records_total_local / total_buckets;
    num_records_in_shuffled_bucket = num_records_in_bucket; // no shuffling in merge-only
    rounds = 1; // merge-only reads full plots
  }

  unsigned long long records_per_global_bucket =
      TOTAL_FILES * num_records_in_bucket;
  unsigned long long global_bucket_size =
      records_per_global_bucket * sizeof(MemoTable2Record);

  // Number of global buckets that can be stored in memory
  unsigned long long total_global_buckets = (unsigned long long)floor(
      BATCH_MEMORY_MB * 1024.0 * 1024.0 / global_bucket_size);

  // Allocate memory
  MemoTable2Record *mergedBuckets = NULL;
  FileRecords file_records[TOTAL_FILES];

  if (MERGE_APPROACH != 2) {
    mergedBuckets = (MemoTable2Record *)calloc(total_global_buckets *
                                                   records_per_global_bucket,
                                               sizeof(MemoTable2Record));

    if (mergedBuckets == NULL) {
      fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < TOTAL_FILES; i++) {
      file_records[i].records = (MemoTable2Record *)calloc(
          total_global_buckets * num_records_in_bucket,
          sizeof(MemoTable2Record));

      if (file_records[i].records == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  // Merge file
  char merge_filename[4096];
  char merge_basename[256];

  // Ensure destination directory exists (mkdir -p semantics)
  if (ensure_folder_exists_recursive(DESTINATION) != 0) {
    fprintf(stderr, "Error: Could not create destination directory '%s'\n",
            DESTINATION);
    return EXIT_FAILURE;
  }

  // Attempt to create a unique merge filename. If merge_K_N.plot exists,
  // append _1, _2, ... until a free name is found (up to a reasonable limit).
  int attempt = 0;
  const int MAX_ATTEMPTS = 10000;
  int merge_fd = -1;
  while (attempt < MAX_ATTEMPTS) {
    if (attempt == 0) {
      snprintf(merge_basename, sizeof(merge_basename), "merge_%d_%d.plot",
               K, TOTAL_FILES);
    } else {
      snprintf(merge_basename, sizeof(merge_basename), "merge_%d_%d_%d.plot",
               K, TOTAL_FILES, attempt);
    }

    path_join(merge_filename, sizeof(merge_filename), DESTINATION,
              merge_basename);

    // Try to create file atomically to avoid races
    merge_fd = open(merge_filename, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (merge_fd != -1) {
      // created successfully
      break;
    } else {
      if (errno == EEXIST) {
        // file exists, try next suffix
        attempt++;
        continue;
      } else {
        // other error
        perror("Error creating merge file");
        return EXIT_FAILURE;
      }
    }
  }

  if (merge_fd == -1) {
    fprintf(stderr, "Error: Unable to create unique merge filename after %d attempts\n",
            MAX_ATTEMPTS);
    return EXIT_FAILURE;
  }

  unsigned long long num_records = 1ULL << K;
  unsigned long long record_size = sizeof(MemoTable2Record);
  unsigned long long size =
      num_records * record_size * TOTAL_FILES + 36 * TOTAL_FILES;
  unsigned long long total_batches =
      ceil((double)total_buckets / total_global_buckets);
    int total_batches_int = (int)total_batches;

  {
    const char *aname = (MERGE_APPROACH == 0) ? "Pipelined"
                      : (MERGE_APPROACH == 1) ? "Serial"
                      :                         "Tasks";

    double per_file_batch_mb = (double)total_global_buckets
                                * num_records_in_bucket
                                * sizeof(MemoTable2Record)
                                / (1024.0 * 1024.0);
    double file_size_mb  = (double)(1ULL << K) * sizeof(MemoTable2Record)
                            / (1024.0 * 1024.0);
    double total_data_gb = (double)TOTAL_FILES * file_size_mb / 1024.0;

    int ma = (int)floor((double)MEMORY_LIMIT_MB / BATCH_MEMORY_MB) - 1;
    if (ma < 1) ma = 1;
    double peak_mem_mb = (MERGE_APPROACH == 2)
                         ? (double)ma * 2.0 * BATCH_MEMORY_MB
                         : 2.0 * BATCH_MEMORY_MB;

    BPRINTF("=== Merge Configuration ===\n");
    BPRINTF("Approach          : %s (%d)\n", aname, MERGE_APPROACH);
    BPRINTF("Files             : %d x K%d\n", TOTAL_FILES, K);
    BPRINTF("File size         : %.2f MB (%.2f GB)\n",
            file_size_mb, file_size_mb / 1024.0);
    BPRINTF("Total data        : %.2f GB\n", total_data_gb);
    BPRINTF("Source            : %s\n", SOURCE);
    BPRINTF("Destination       : %s\n", DESTINATION);
    BPRINTF("\n=== Batch Memory (-B) ===\n");
    BPRINTF("Batch size        : %d MB\n", BATCH_MEMORY_MB);
    BPRINTF("Per-file / batch  : %.2f MB\n", per_file_batch_mb);
    BPRINTF("Expected peak RAM : %.0f MB (%.2f GB)  [2 x B%s]\n",
            peak_mem_mb, peak_mem_mb / 1024.0,
            MERGE_APPROACH == 2 ? " x max_active_batches" : "");
    BPRINTF("Total batches     : %llu\n", total_batches);
    BPRINTF("Buckets / batch   : %llu\n", total_global_buckets);
    if (MERGE_APPROACH == 2) {
      int raw_ma = (int)floor((double)MEMORY_LIMIT_MB / BATCH_MEMORY_MB) - 1;
      BPRINTF("Max active batches: %d  (MEMORY_LIMIT %d MB / B %d MB - 1)\n",
              raw_ma, MEMORY_LIMIT_MB, BATCH_MEMORY_MB);
      if (raw_ma <= 0) {
        BPRINTF("WARNING: max_active_batches=%d — B (%d MB) >= MEMORY_LIMIT (%d MB)."
                " Increase -m to avoid deadlock with -A tasks.\n",
                raw_ma, BATCH_MEMORY_MB, MEMORY_LIMIT_MB);
      }
    }
    BPRINTF("\n=== Threads ===\n");
    BPRINTF("Compute (-t)      : %d\n", num_threads);
    BPRINTF("I/O (-mt)         : %d\n", MERGE_IO_THREADS);
    BPRINTF("Memory limit (-m) : %d MB  (tasks pipeline budget)\n", MEMORY_LIMIT_MB);
    BPRINTF("\n");
  }

  double t_start = omp_get_wtime();
#if defined(__APPLE__)
  int ret = ftruncate(merge_fd, size);
#else
  int ret = posix_fallocate(merge_fd, 0, size);
#endif
  double t_end = omp_get_wtime();

  if (ret != 0) {
#if defined(__APPLE__)
    perror("ftruncate");
#else
    errno = ret;
    perror("posix_fallocate");
#endif
    close(merge_fd);
    return 1;
  }

#if defined(__APPLE__)
  const char *alloc_call = "ftruncate";
#else
  const char *alloc_call = "posix_fallocate";
#endif
  BPRINTF("%s (%llu bytes) took %.3f seconds\n\n\n", alloc_call, size,
          t_end - t_start);

  // Preload Files
  FILE *files[TOTAL_FILES];
  int fds[TOTAL_FILES];

  for (int i = 0; i < TOTAL_FILES; i++) {
    char *filename = filenames[i];

    files[i] = fopen(filename, "rb");
    if (!files[i]) {
      perror("Error opening file");
      return 1;
    }

    fds[i] = open(filename, O_RDONLY);
    if (fds[i] < 0) {
      perror("Error opening file");
      return 1;
    }
  }

  unsigned long long end;
  double batch_start_time;
  double start_time = omp_get_wtime();
  double read_total_time = 0;
  double merge_total_time = 0.0;
  double write_total_time = 0;
  double read_time = 0.0;
  double write_time = 0.0;

  switch (MERGE_APPROACH) {

  // Parallel Read & Merge
  // FIXME: Time benchamrking
  case 0: {

    for (unsigned long long i = 0; i < total_buckets;
         i += total_global_buckets) {

      end = i + total_global_buckets;
      if (end > total_buckets) {
        end = total_buckets;
      }

      batch_start_time = omp_get_wtime();

#pragma omp parallel for num_threads(MERGE_IO_THREADS) schedule(static)
      for (int f = 0; f < TOTAL_FILES; f++) {
        FILE *fd = files[f];

        size_t read_bytes =
            fread(file_records[f].records, sizeof(MemoTable2Record),
                  (end - i) * num_records_in_bucket, fd);
        if (read_bytes != num_records_in_bucket * (end - i)) {
          if (feof(fd)) {
            BPRINTF("Reached end of file after reading %zu bytes\n",
                   read_bytes);
          } else {
            perror("fread failed");
            fclose(fd);
          }
        }
      }

      read_time = omp_get_wtime() - batch_start_time;
      read_total_time += read_time;

#pragma omp parallel for num_threads(num_threads) schedule(static)
      for (unsigned long long k = 0; k < end - i; k++) {
        for (int f = 0; f < TOTAL_FILES; f++) {
          FileRecords *file_record = &file_records[f];
          unsigned long long off = (unsigned long long)f * num_records_in_bucket;
          memcpy(&mergedBuckets[k * records_per_global_bucket + off],
                 &file_record->records[k * num_records_in_bucket],
                 num_records_in_bucket * sizeof(MemoTable2Record));
        }
      }

      double merge_done_time = omp_get_wtime();
      merge_total_time += merge_done_time - batch_start_time - read_time;

      double write_start_time = omp_get_wtime();
      size_t total_bytes =
          (end - i) * records_per_global_bucket * sizeof(MemoTable2Record);
      size_t bytes_written = 0;

      while (bytes_written < total_bytes) {
        ssize_t res = write(merge_fd, (char *)mergedBuckets + bytes_written,
                            total_bytes - bytes_written);
        if (res < 0) {
          perror("Error writing to merge file");
          close(merge_fd);
          exit(EXIT_FAILURE);
        }
        bytes_written += res;
      }
      double now = omp_get_wtime();
      /* These two used to be locals shadowing the outer read_time/write_time,
         so write_total_time never saw a per-batch write -- it only ever picked
         up the final fsync, and the whole write cost silently landed in the
         interleave accumulator. read_time is already correct from above. */
      write_time = now - write_start_time;
      write_total_time += write_time;
      double batch_time = now - batch_start_time;
      double total_time = now - start_time;
      double batch_throughput_MBps =
          batch_time > 0 ? ((double)total_bytes / (1024.0 * 1024.0)) / batch_time
                          : 0.0;
      double eta_seconds =
          ((double)end / total_buckets) > 0
              ? total_time * (1 - ((double)end / total_buckets)) /
                    ((double)end / total_buckets)
              : 0.0;
      BPRINTF("[%.2f%%] | Read: %.4fs | Write: %.4fs | Batch Time: %.6fs | Total Time: %.2fs | ETA: %.2fs | Throughput: %.2f MB/s\n",
              ((double)end / total_buckets) * 100, read_time, write_time,
              batch_time, total_time, eta_seconds, batch_throughput_MBps);
    }

    break;
  }

  // Parallel Merge
  case 1: {

    for (unsigned long long i = 0; i < total_buckets;
         i += total_global_buckets) {

      end = i + total_global_buckets;
      if (end > total_buckets) {
        end = total_buckets;
      }

      batch_start_time = omp_get_wtime();

#pragma omp parallel for num_threads(MERGE_IO_THREADS) schedule(static)
      for (int f = 0; f < TOTAL_FILES; f++) {
        int fd = fds[f]; // Assumes files are opened with open()

        size_t total_bytes =
            (end - i) * num_records_in_bucket * sizeof(MemoTable2Record);
        char *buffer = (char *)file_records[f].records;

        size_t bytes_read = 0;
        while (bytes_read < total_bytes) {
          ssize_t n = read(fd, buffer + bytes_read, total_bytes - bytes_read);
          if (n < 0) {
            perror("read failed");
            close(fd);
            break;
          } else if (n == 0) {
            BPRINTF("Reached end of file after reading %zu bytes (expected %zu)\n",
                    bytes_read, total_bytes);
            break;
          }
          bytes_read += n;
        }

        if (bytes_read != total_bytes) {
          fprintf(stderr,
                  "Warning: Partial read for file %d (got %zu, expected %zu)\n",
                  f, bytes_read, total_bytes);
        }
      }

      read_time = omp_get_wtime() - batch_start_time;
      read_total_time += read_time;

#pragma omp parallel for num_threads(num_threads) schedule(static)
      for (unsigned long long k = 0; k < end - i; k++) {
        for (int f = 0; f < TOTAL_FILES; f++) {
          FileRecords *file_record = &file_records[f];
          unsigned long long off = (unsigned long long)f * num_records_in_bucket;
          memcpy(&mergedBuckets[k * records_per_global_bucket + off],
                 &file_record->records[k * num_records_in_bucket],
                 num_records_in_bucket * sizeof(MemoTable2Record));
        }
      }

      double write_start_time = omp_get_wtime();

      size_t total_bytes =
          (end - i) * records_per_global_bucket * sizeof(MemoTable2Record);
      size_t bytes_written = 0;

      while (bytes_written < total_bytes) {
        ssize_t res = write(merge_fd, (char *)mergedBuckets + bytes_written,
                            total_bytes - bytes_written);
        if (res < 0) {
          perror("Error writing to merge file");
          close(merge_fd);
          exit(EXIT_FAILURE);
        }
        bytes_written += res;
      }

      write_time = omp_get_wtime() - write_start_time;
      write_total_time += write_time;

      double now = omp_get_wtime();
      double batch_time = now - batch_start_time;
      double total_time = now - start_time;
      double batch_throughput_MBps =
          batch_time > 0 ? ((double)total_bytes / (1024.0 * 1024.0)) / batch_time
                          : 0.0;
      double eta_seconds =
          ((double)end / total_buckets) > 0
              ? total_time * (1 - ((double)end / total_buckets)) /
                    ((double)end / total_buckets)
              : 0.0;
      BPRINTF("[%.2f%%] | Read: %.4fs | Write: %.4fs | Batch Time: %.6fs | Total Time: %.2fs | ETA: %.2fs | Throughput: %.2f MB/s\n",
              ((double)end / total_buckets) * 100, read_time, write_time,
              batch_time, total_time, eta_seconds, batch_throughput_MBps);
    }

    break;
  }

  case 2: {
    MergeBatch *mergeBatches =
        (MergeBatch *)malloc(sizeof(MergeBatch) * total_batches);

    if (!mergeBatches) {
      perror("Failed to allocate mergeBatches");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < total_batches_int; i++) {
      mergeBatches[i].readDone = false;
      mergeBatches[i].mergeDone = false;
      mergeBatches[i].writeDone = false;
      mergeBatches[i].total_time = 0;
    }

    // During the merge, an extra temporary buffer is created
    // We account for it by reducing one batch from the total allowed limit
    // Only one merge is going on a time, which we assume is always active
    int max_active_batches =
        floor((double)MEMORY_LIMIT_MB / BATCH_MEMORY_MB) - 1;
    max_active_batches =
      max_active_batches < total_batches_int ? max_active_batches : total_batches_int;

#pragma omp parallel

    {

#pragma omp single

      {
        for (int batch_idx = 0; batch_idx < total_batches_int; batch_idx++) {
          unsigned long long start_bucket = total_global_buckets * batch_idx;
          unsigned long long end_bucket =
              (batch_idx == total_batches_int - 1)
                  ? total_buckets
                  : start_bucket + total_global_buckets;
          unsigned long long buckets_in_batch = end_bucket - start_bucket;

          // Create Read Tasks
          if (batch_idx == 0) {
#pragma omp task depend(out : mergeBatches[batch_idx].readDone)
            {
              batch_read(batch_idx);
            }
          } else if (batch_idx >= max_active_batches) {
#pragma omp task depend(                                                       \
        in : mergeBatches[batch_idx - 1].readDone,                             \
            mergeBatches[batch_idx - max_active_batches].writeDone)            \
    depend(out : mergeBatches[batch_idx].readDone)
            {
              batch_read(batch_idx);
            }
          } else {
#pragma omp task depend(in : mergeBatches[batch_idx - 1].readDone)             \
    depend(out : mergeBatches[batch_idx].readDone)
            {
              batch_read(batch_idx);
            }
          }

          // Create Merge Tasks
#pragma omp task depend(in : mergeBatches[batch_idx].readDone)                 \
    depend(out : mergeBatches[batch_idx].mergeDone)
          {
            if (DEBUG) {
              BPRINTF("[%d] Merge Started\n", batch_idx);
            }

            double start_time = omp_get_wtime();

            MergeBatch *mergeBatch = &mergeBatches[batch_idx];
            mergeBatch->mergedBuckets = (MemoTable2Record *)malloc(
                buckets_in_batch * global_bucket_size);

            if (mergeBatch->mergedBuckets == NULL) {
              fprintf(stderr,
                      "Error: Failed to allocate memory for mergedBuckets "
                      "(batch %d)\n",
                      batch_idx);
              exit(EXIT_FAILURE);
            }

            // FIXME: Find better way to utilize available threads for merging
            // How many are available? Do we want to exhaust all of them?
            int total_merge_threads = omp_get_num_threads() >= 16
                                          ? omp_get_num_threads() - 32
                                          : omp_get_num_threads() / 2;

#pragma omp taskloop
            for (int merge_thread = 0; merge_thread < total_merge_threads;
                 merge_thread++) {
        unsigned long long batch_size =
          (unsigned long long)floor((double)buckets_in_batch /
                       total_merge_threads);
        unsigned long long merge_start_bucket =
          batch_size * (unsigned long long)merge_thread;
        unsigned long long merge_end_bucket =
          (merge_thread == total_merge_threads - 1)
            ? buckets_in_batch
            : merge_start_bucket + batch_size;

        for (unsigned long long bucket_idx = merge_start_bucket;
                   bucket_idx < merge_end_bucket; bucket_idx++) {
                for (int f = 0; f < TOTAL_FILES; f++) {
                  memcpy(
                      &mergeBatch->mergedBuckets[bucket_idx *
                                                     records_per_global_bucket +
                                                 f * num_records_in_bucket],
                      &mergeBatch->buffer[f * buckets_in_batch *
                                              num_records_in_bucket +
                                          bucket_idx * num_records_in_bucket],
                      num_records_in_bucket * sizeof(MemoTable2Record));
                }
              }
            }

            free(mergeBatches[batch_idx].buffer);

            double elapsed = omp_get_wtime() - start_time;
            mergeBatch->total_time += elapsed;
            merge_total_time += elapsed;

            mergeBatch->mergeDone = true;

            if (DEBUG) {
              BPRINTF("[%d] Merge Complete: %.2f\n", batch_idx, elapsed);
            }
          }

          // Create Write Tasks
          if (batch_idx == 0) {
#pragma omp task depend(in : mergeBatches[batch_idx].mergeDone)                \
    depend(out : mergeBatches[batch_idx].writeDone)
            {
              batch_write(batch_idx);
            }
          } else {
#pragma omp task depend(in : mergeBatches[batch_idx].mergeDone,                \
                            mergeBatches[batch_idx - 1].writeDone)             \
    depend(out : mergeBatches[batch_idx].writeDone)
            {
              batch_write(batch_idx);
            }
          }
        }
      }
    }

    break;
  }

  default: {
    break;
  }
  }

  // Write metadata to footer
  double finalize_start_time = omp_get_wtime();

  size_t total_bytes = sizeof(PlotData) * TOTAL_FILES;
  size_t bytes_written = 0;

  while (bytes_written < total_bytes) {
    ssize_t res = write(merge_fd, (char *)plotData + bytes_written,
                        total_bytes - bytes_written);
    if (res < 0) {
      perror("Failed to write metadata");
      close(merge_fd);
      return EXIT_FAILURE;
    }
    bytes_written += res;
  }

  // Sync disk
  if (fsync(merge_fd) != 0) {
    perror("Failed to fsync");
    close(merge_fd);
    return EXIT_FAILURE;
  }

  if (close(merge_fd) != 0) {
    perror("Failed to close");
    return EXIT_FAILURE;
  }

  write_total_time += omp_get_wtime() - finalize_start_time;

  double process_time = omp_get_wtime() - start_time;

  /* Approaches that do not time the interleave directly (serial) leave the
     accumulator at zero; back it out of the total instead. */
  if (merge_total_time <= 0.0) {
    merge_total_time = process_time - read_total_time - write_total_time;
    if (merge_total_time < 0.0) merge_total_time = 0.0;
  }

  /* Set global merge timing variables so the caller can print an aggregate
     benchmark line. */
  merge_read_time = read_total_time;
  merge_write_time = write_total_time;
  merge_compute_time = merge_total_time;
  merge_total_time = process_time;

  {
    const char *aname = (MERGE_APPROACH == 0) ? "Pipelined"
                      : (MERGE_APPROACH == 1) ? "Serial"
                      :                         "Tasks";
    double total_size_gb  = (double)size / (1024.0 * 1024.0 * 1024.0);
    double avg_throughput = (process_time > 0)
                            ? ((double)size / (1024.0 * 1024.0)) / process_time
                            : 0.0;
    double peak_mem_mb    = (MERGE_APPROACH == 2)
                            ? (double)(MEMORY_LIMIT_MB / BATCH_MEMORY_MB - 1) * 2.0 * BATCH_MEMORY_MB
                            : 2.0 * BATCH_MEMORY_MB;
    if (peak_mem_mb < 0) peak_mem_mb = 2.0 * BATCH_MEMORY_MB;

    BPRINTF("\n\n");
    BPRINTF("[%.2fs] Completed merging %d K%d-files of total size %.2fGB\n",
            process_time, TOTAL_FILES, K, total_size_gb);
    BPRINTF("Merge Approach: %s\n", aname);
    BPRINTF("Source: %s\n", SOURCE);
    BPRINTF("Destination: %s\n", DESTINATION);
    BPRINTF("Read Time: %.2fs\n", merge_read_time);
    BPRINTF("Write Time: %.2fs\n", merge_write_time);
    BPRINTF("Interleave Time: %.2fs\n", merge_compute_time);
    BPRINTF("Total Merge Time: %.2fs\n", merge_total_time);
    /* Deprecated alias: "Merge Time" has always carried the *total*, not the
       interleave. Kept so older parsers keep reading what they always read. */
    BPRINTF("Merge Time: %.2fs\n", merge_total_time);
    BPRINTF("Avg Throughput: %.2f MB/s\n", avg_throughput);
    /* Modelled, not measured -- 2xB double buffer. */
    BPRINTF("Expected Peak RAM: %.0f MB\n", peak_mem_mb);
    BPRINTF("Peak Memory Usage: %.2f MB\n\n\n", merge_peak_rss_mb());
  }

  if (MERGE_APPROACH != 2) {
    free(mergedBuckets);

    for (int i = 0; i < TOTAL_FILES; i++) {
      free(file_records[i].records);
    }
  }

  // FIXME: Don't delete it!!
  // remove(merge_fd);

  for (int i = 0; i < TOTAL_FILES; i++) {
    fclose(files[i]);
    close(fds[i]);
  }

  // Verify complete merge plot
  if (VERIFY) {
    printf("Starting verification: \n");

    size_t verified_global_buckets = 0;

#pragma omp parallel for
    for (unsigned long long i = 0; i < total_buckets; i += BATCH_SIZE) {

      FILE *fd = fopen(merge_filename, "rb");
      if (!fd) {
        perror("Error opening file");
      }

      MemoTable2Record *buffer = calloc(BATCH_SIZE * records_per_global_bucket,
                                        sizeof(MemoTable2Record));

      size_t offset = i * global_bucket_size;

      fseek(fd, offset, SEEK_SET);
      size_t elements_written =
          fread(buffer, sizeof(MemoTable2Record),
                BATCH_SIZE * records_per_global_bucket, fd);

      if (elements_written != BATCH_SIZE * records_per_global_bucket) {
        fprintf(stderr, "Error writing bucket to file");
        fclose(fd);
        exit(EXIT_FAILURE);
      }

      size_t end = i + BATCH_SIZE;
      if (end > total_buckets) {
        end = total_buckets;
      }

      MemoTable2Record *record;
      uint8_t hash[HASH_SIZE];

      for (unsigned long long j = i; j < end; j++) {
        bool flag = true;

        for (size_t k = 0; k < records_per_global_bucket; k++) {
          record = &buffer[(j - i) * records_per_global_bucket + k];

          if (byteArrayToLongLong(record->nonce1, NONCE_SIZE) != 0 ||
              byteArrayToLongLong(record->nonce2, NONCE_SIZE) != 0) {
            generateBlake3Pair(record->nonce1, record->nonce2,
                               plotData[k / num_records_in_bucket].key, hash);

            if (byteArrayToLongLong(hash, PREFIX_SIZE) != j) {
              flag = false;
              break;
            }
          }
        }

        if (flag) {
#pragma omp atomic
          verified_global_buckets++;
        }
      }

      free(buffer);

      fclose(fd);
    }

        printf("Total Buckets: %llu\nVerified Buckets: %zu\n", total_buckets,
          verified_global_buckets);
  }
  return 0;
}
