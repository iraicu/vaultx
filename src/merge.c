#define _GNU_SOURCE
#include "merge.h"
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

void print_numa_node(void* ptr, const char* label) {
    int status;
    int ret = get_mempolicy(&status, NULL, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR);
    if (ret != 0) {
        perror("get_mempolicy");
        exit(EXIT_FAILURE);
    }
    printf("%s is on NUMA node %d\n", label, status);
}

int get_current_cpu() {
    return sched_getcpu(); // Returns the CPU number the calling thread is running on
}

void pin_thread_to_cpu(int cpu_num) {
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
}

#define batch_read(batch_idx)                                                                                                   \
    do {                                                                                                                        \
        if (DEBUG) {                                                                                                            \
            printf("[%d] Read Started\n", batch_idx);                                                                           \
        }                                                                                                                       \
                                                                                                                                \
        double start_time = omp_get_wtime();                                                                                    \
                                                                                                                                \
        MergeBatch* mergeBatch = &mergeBatches[batch_idx];                                                                      \
        mergeBatch->buffer = (MemoTable2Record*)calloc(buckets_in_batch * records_per_global_bucket, sizeof(MemoTable2Record)); \
                                                                                                                                \
        if (mergeBatch->buffer == NULL) {                                                                                       \
            fprintf(stderr, "Error: Failed to allocate memory for buckets (batch %d)\n", batch_idx);                            \
            exit(EXIT_FAILURE);                                                                                                 \
        }                                                                                                                       \
                                                                                                                                \
        char* buf = (char*)mergeBatch->buffer;                                                                                  \
                                                                                                                                \
        for (int f = 0; f < TOTAL_FILES; f++) {                                                                                 \
            int fd = fds[f];                                                                                                    \
            size_t total_bytes = buckets_in_batch * num_records_in_bucket * sizeof(MemoTable2Record);                           \
            size_t bytes_read = 0;                                                                                              \
            char* dest = buf + f * total_bytes;                                                                                 \
                                                                                                                                \
            while (bytes_read < total_bytes) {                                                                                  \
                ssize_t res = read(fd, dest + bytes_read, total_bytes - bytes_read);                                            \
                if (res < 0) {                                                                                                  \
                    perror("read failed");                                                                                      \
                    close(fd);                                                                                                  \
                    break;                                                                                                      \
                } else if (res == 0) {                                                                                          \
                    fprintf(stderr, "Unexpected EOF on file %d\n", f);                                                          \
                    break;                                                                                                      \
                }                                                                                                               \
                bytes_read += res;                                                                                              \
            }                                                                                                                   \
                                                                                                                                \
            if (bytes_read < total_bytes) {                                                                                     \
                fprintf(stderr, "Only read %zu of %zu bytes from file %d\n", bytes_read, total_bytes, f);                       \
            }                                                                                                                   \
        }                                                                                                                       \
                                                                                                                                \
        double elapsed = omp_get_wtime() - start_time;                                                                          \
        mergeBatch->total_time += elapsed;                                                                                      \
        read_total_time += elapsed;                                                                                             \
                                                                                                                                \
        if (DEBUG) {                                                                                                            \
            printf("[%d] Read Complete: %.2fs\n", batch_idx, elapsed);                                                          \
        }                                                                                                                       \
                                                                                                                                \
        mergeBatch->readDone = true;                                                                                            \
    } while (0)

#define batch_write(batch_idx)                                                                        \
    do {                                                                                              \
        if (DEBUG) {                                                                                  \
            printf("[%d] Write Started\n", batch_idx);                                                \
        }                                                                                             \
                                                                                                      \
        MergeBatch* mergeBatch = &mergeBatches[batch_idx];                                            \
        double write_start_time = omp_get_wtime();                                                    \
                                                                                                      \
        size_t total_bytes = buckets_in_batch * records_per_global_bucket * sizeof(MemoTable2Record); \
        size_t bytes_written = 0;                                                                     \
        char* src = (char*)mergeBatch->mergedBuckets;                                                 \
                                                                                                      \
        while (bytes_written < total_bytes) {                                                         \
            ssize_t res = write(merge_fd, src + bytes_written, total_bytes - bytes_written);          \
            if (res < 0) {                                                                            \
                perror("Error writing to merge file");                                                \
                close(merge_fd);                                                                      \
                exit(EXIT_FAILURE);                                                                   \
            } else if (res == 0) {                                                                    \
                fprintf(stderr, "Unexpected zero write\n");                                           \
                break;                                                                                \
            }                                                                                         \
            bytes_written += res;                                                                     \
        }                                                                                             \
                                                                                                      \
        if (bytes_written < total_bytes) {                                                            \
            fprintf(stderr, "Only wrote %zu of %zu bytes\n", bytes_written, total_bytes);             \
        }                                                                                             \
                                                                                                      \
        free(mergeBatch->mergedBuckets);                                                              \
                                                                                                      \
        double write_time = omp_get_wtime() - write_start_time;                                       \
        write_total_time += write_time;                                                               \
        mergeBatch->total_time += write_time;                                                         \
                                                                                                      \
        if (DEBUG) {                                                                                  \
            printf("[%d] Write Complete: %.2fs\n", batch_idx, write_time);                            \
        }                                                                                             \
                                                                                                      \
        double write_throughput_MBps = BATCH_MEMORY_MB / write_time;                                  \
        double progress = ((double)end_bucket / total_buckets);                                       \
        double elapsed_time = omp_get_wtime() - start_time;                                           \
        printf("[%6.2f%%] | Batch %-6d (%9.2fs) | Total: %9.2fs | Time Left: %9.2fs\n",               \
            progress * 100,                                                                           \
            batch_idx,                                                                                \
            mergeBatch->total_time,                                                                   \
            elapsed_time,                                                                             \
            elapsed_time * (1 - progress) / progress);                                                \
    } while (0)

void merge() {

    int MAX_FILENAME_LEN = 256;
    char filenames[TOTAL_FILES][MAX_FILENAME_LEN];
    char dir_name[256];
    snprintf(dir_name, sizeof(dir_name), "/%s/arnav/vaultx/plots", SOURCE);

    DIR* d = opendir(dir_name);
    struct dirent* dir;

    int count = 0;

    if (d) {
        while ((dir = readdir(d)) != NULL && count < TOTAL_FILES) {
            if (dir->d_name[0] != '.') { // skip hidden files
                // Build full path "plots/filename"
                snprintf(filenames[count], MAX_FILENAME_LEN, "%s/%s", dir_name, dir->d_name);
                filenames[count][MAX_FILENAME_LEN - 1] = '\0'; // safety null-terminate
                count++;
            }
        }
        closedir(d);
    } else {
        perror("opendir");
        return 1;
    }

    PlotData plotData[TOTAL_FILES];

    for (int i = 0; i < TOTAL_FILES; i++) {
        char* filename = filenames[i];

        // Step 1: Skip dir_name and '/'
        filename += strlen(dir_name);
        if (*filename == '/') {
            filename++; // skip slash if present
        }

        // Step 2: Parse
        int kVal = 0;
        char hex[129]; // large buffer just in case
        if (sscanf(filename, "K%d_%128[^.].plot", &kVal, hex) != 2) {
            printf("Parsing failed\n");
        }

        uint8_t* plot_id = hexStringToByteArray(hex);

        plotData[i].K = kVal;
        K = kVal;
        derive_key(kVal, plot_id, plotData[i].key);
    }

    unsigned long long records_per_global_bucket = TOTAL_FILES * num_records_in_bucket;
    unsigned long long global_bucket_size = records_per_global_bucket * sizeof(MemoTable2Record);

    // Number of global buckets that can be stored in memory
    unsigned long long total_global_buckets = (unsigned long long)floor(BATCH_MEMORY_MB * 1024.0 * 1024.0 / global_bucket_size);

    // Allocate memory
    MemoTable2Record* mergedBuckets;
    FileRecords file_records[TOTAL_FILES];

    if (MERGE_APPROACH != 2) {
        mergedBuckets = (MemoTable2Record*)calloc(total_global_buckets * records_per_global_bucket, sizeof(MemoTable2Record));

        if (mergedBuckets == NULL) {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < TOTAL_FILES; i++) {
            file_records[i].records = (MemoTable2Record*)calloc(total_global_buckets * num_records_in_bucket, sizeof(MemoTable2Record));

            if (file_records[i].records == NULL) {
                fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Merge file
    char merge_filename[100];
    snprintf(merge_filename, sizeof(merge_filename), "/%s/arnav/vaultx/merge_%d_%d.plot", DESTINATION, K, TOTAL_FILES);
    remove(merge_filename);

    int merge_fd = open(merge_filename, O_WRONLY | O_CREAT, 0644);

    if (merge_fd == -1) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    unsigned long long num_records = 1ULL << K;
    unsigned long long record_size = sizeof(MemoTable2Record);
    unsigned long long size = num_records * record_size * TOTAL_FILES + 36 * TOTAL_FILES;
    unsigned long long total_batches = ceil((double)total_buckets / total_global_buckets);

    printf("Memory Size per Batch: %lluMB\n", BATCH_MEMORY_MB);
    printf("Buckets processed from each file per batch: %llu\n", total_global_buckets);
    printf("Bucket size: %llu bytes\n", num_records_in_bucket * sizeof(MemoTable2Record));
    printf("Data read from each file per batch: %.2fMB\n\n", (double)total_global_buckets * num_records_in_bucket * sizeof(MemoTable2Record) / (1024 * 1024));
    printf("Total Batches: %llu\n", total_batches);

    printf("Merge Approach [%d]: %s\n", MERGE_APPROACH, MERGE_APPROACH == 1 ? "Serial" : "Pipelined");
    printf("Source: %s\n", SOURCE);
    printf("Destination: %s\n", DESTINATION);
    printf("Threads: %d\n", num_threads);
    printf("Files: %d, K%d\n", TOTAL_FILES, K);
    printf("Memory Limit: %lluMB\n\n\n", MEMORY_LIMIT_MB);

    double t_start = omp_get_wtime();
    int ret = posix_fallocate(merge_fd, 0, size);
    double t_end = omp_get_wtime();

    if (ret != 0) {
        perror("posix_fallocate");
        close(merge_fd);
        return 1;
    }

    printf("posix_fallocate (%llu bytes) took %.3f seconds\n\n\n", size, t_end - t_start);

    // Preload Files
    FILE* files[TOTAL_FILES];
    int fds[TOTAL_FILES];

    for (int i = 0; i < TOTAL_FILES; i++) {
        char* filename = filenames[i];

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
    double batch_start_time, merge_start_time;
    double start_time = omp_get_wtime();
    double read_total_time = 0;
    double merge_total_time = 0.0;
    double write_total_time = 0;

    switch (MERGE_APPROACH) {

    // Parallel Read & Merge
    // FIXME: Time benchamrking
    case 0: {

#pragma omp parallel
        {
            for (unsigned long long i = 0; i < total_buckets; i += total_global_buckets) {

                int tid = omp_get_thread_num();

                if (tid == 0) {
                    end = i + total_global_buckets;
                    if (end > total_buckets) {
                        end = total_buckets;
                    }

                    batch_start_time = omp_get_wtime();
                }

#pragma omp barrier

#pragma omp for

                for (int f = 0; f < TOTAL_FILES; f++) {
                    FILE* fd = files[f];

                    size_t read_bytes = fread(file_records[f].records, sizeof(MemoTable2Record), (end - i) * num_records_in_bucket, fd);
                    if (read_bytes != num_records_in_bucket * (end - i)) {
                        if (feof(fd)) {
                            printf("Reached end of file after reading %zu bytes\n", read_bytes);
                        } else {
                            perror("fread failed");
                            fclose(fd);
                            // return 1;
                        }
                    }

                    FileRecords* file_record = &file_records[f];
                    unsigned long long off = f * num_records_in_bucket;
                    for (unsigned long long k = 0; k < end - i; k++) {
                        memcpy(&mergedBuckets[k * records_per_global_bucket + off], &file_record->records[k * num_records_in_bucket], num_records_in_bucket * sizeof(MemoTable2Record));
                    }
                }

                if (tid == 0) {
                    printf("Read: %.4f\n", omp_get_wtime() - batch_start_time);
                    double fs = omp_get_wtime();
                    size_t total_bytes = (end - i) * records_per_global_bucket * sizeof(MemoTable2Record);
                    size_t bytes_written = 0;

                    while (bytes_written < total_bytes) {
                        ssize_t res = write(merge_fd, (char*)mergedBuckets + bytes_written, total_bytes - bytes_written);
                        if (res < 0) {
                            perror("Error writing to merge file");
                            close(merge_fd);
                            exit(EXIT_FAILURE);
                        }
                        bytes_written += res;
                    }
                    printf("Write: %.4f\n", omp_get_wtime() - fs);

                    printf("[%.2f%%] | Batch Time: %.6fs | Total Time: %.2fs\n", ((double)end / total_buckets) * 100, omp_get_wtime() - batch_start_time, omp_get_wtime() - start_time);
                }
            }
        }

        break;
    }

    // Parallel Merge
    case 1: {

#pragma omp parallel
        {

            // pin_thread_to_cpu(omp_get_thread_num());
            // printf("CPU: %d\n", get_current_cpu());

            // #pragma omp barrier

            for (unsigned long long i = 0; i < total_buckets; i += total_global_buckets) {

#pragma omp single

                {
                    // if (i == 0) {
                    //  int node = 2; // NUMA node to allocate on

                    // if (MERGE_APPROACH != 2) {
                    //     size_t merged_size = total_global_buckets * records_per_global_bucket * sizeof(MemoTable2Record);
                    //     mergedBuckets = (MemoTable2Record*)numa_alloc_onnode(merged_size, node);
                    //     if (!mergedBuckets) {
                    //         fprintf(stderr, "Error: Unable to allocate memory for buckets on NUMA node %d.\n", node);
                    //         exit(EXIT_FAILURE);
                    //     }
                    //     memset(mergedBuckets, 0, merged_size);
                    //     print_numa_node(mergedBuckets, "mergedBuckets");

                    //     for (int i = 0; i < TOTAL_FILES; i++) {
                    //         size_t file_size = total_global_buckets * num_records_in_bucket * sizeof(MemoTable2Record);
                    //         file_records[i].records = (MemoTable2Record*)numa_alloc_onnode(file_size, node);
                    //         if (!file_records[i].records) {
                    //             fprintf(stderr, "Error: Unable to allocate memory for file_records[%d] on NUMA node %d.\n", i, node);
                    //             exit(EXIT_FAILURE);
                    //         }
                    //         memset(file_records[i].records, 0, file_size);

                    //         char label[64];
                    //         snprintf(label, sizeof(label), "file_records[%d]", i);
                    //         print_numa_node(file_records[i].records, label);
                    //     }
                    // }
                    //}
                    end = i + total_global_buckets;
                    if (end > total_buckets) {
                        end = total_buckets;
                    }

                    batch_start_time = omp_get_wtime();

                    for (int f = 0; f < TOTAL_FILES; f++) {
                        int fd = fds[f]; // Assumes files are opened with open()

                        size_t total_bytes = (end - i) * num_records_in_bucket * sizeof(MemoTable2Record);
                        char* buffer = (char*)file_records[f].records;

                        size_t bytes_read = 0;
                        while (bytes_read < total_bytes) {
                            ssize_t n = read(fd, buffer + bytes_read, total_bytes - bytes_read);
                            if (n < 0) {
                                perror("read failed");
                                close(fd);
                                // return 1;
                                break;
                            } else if (n == 0) {
                                printf("Reached end of file after reading %zu bytes (expected %zu)\n", bytes_read, total_bytes);
                                break;
                            }
                            bytes_read += n;
                        }

                        if (bytes_read != total_bytes) {
                            fprintf(stderr, "Warning: Partial read for file %d (got %zu, expected %zu)\n", f, bytes_read, total_bytes);
                        }
                    }

                    double read_time = omp_get_wtime() - batch_start_time;
                    read_total_time += read_time;

                    double read_throughput_MBps = BATCH_MEMORY_MB / read_time; // MB per second
                    printf("Read : %.4fs, Rate: %.2f MB/s\n", read_time, read_throughput_MBps);

                    merge_start_time = omp_get_wtime();
                }

#pragma omp for
                for (unsigned long long k = 0; k < end - i; k++) {
                    for (int f = 0; f < TOTAL_FILES; f++) {
                        FileRecords* file_record = &file_records[f];
                        unsigned long long off = f * num_records_in_bucket;
                        memcpy(&mergedBuckets[k * records_per_global_bucket + off], &file_record->records[k * num_records_in_bucket], num_records_in_bucket * sizeof(MemoTable2Record));
                    }
                }

#pragma omp single
                {
                    double merge_time = omp_get_wtime() - merge_start_time;

                    printf("Merge: %.4fs\n", merge_time);

                    double write_start_time = omp_get_wtime();

                    size_t total_bytes = (end - i) * records_per_global_bucket * sizeof(MemoTable2Record);
                    size_t bytes_written = 0;

                    while (bytes_written < total_bytes) {
                        ssize_t res = write(merge_fd, (char*)mergedBuckets + bytes_written, total_bytes - bytes_written);
                        if (res < 0) {
                            perror("Error writing to merge file");
                            close(merge_fd);
                            exit(EXIT_FAILURE);
                        }
                        bytes_written += res;
                    }

                    double write_time = omp_get_wtime() - write_start_time;
                    write_total_time += write_time;

                    double write_throughput_MBps = BATCH_MEMORY_MB / write_time; // MB per second
                    printf("Write: %.4fs, Rate: %.2f MB/s\n", write_time, write_throughput_MBps);

                    printf("[%.2f%%] | Batch Time: %.6fs | Total Time: %.2fs\n\n", ((double)end / total_buckets) * 100, omp_get_wtime() - batch_start_time, omp_get_wtime() - start_time);
                }
            }
        }

        break;
    }

    case 2: {
        MergeBatch* mergeBatches = (MergeBatch*)malloc(sizeof(MergeBatch) * total_batches);

        if (!mergeBatches) {
            perror("Failed to allocate mergeBatches");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < total_batches; i++) {
            mergeBatches[i].readDone = false;
            mergeBatches[i].mergeDone = false;
            mergeBatches[i].writeDone = false;
            mergeBatches[i].total_time = 0;
        }

        // During the merge, an extra temporary buffer is created
        // We account for it by reducing one batch from the total allowed limit
        // Only one merge is going on a time, which we assume is always active
        int max_active_batches = floor((double)MEMORY_LIMIT_MB / BATCH_MEMORY_MB) - 1;
        max_active_batches = max_active_batches < total_batches ? max_active_batches : total_batches;

#pragma omp parallel

        {

#pragma omp single

            {
                for (int batch_idx = 0; batch_idx < total_batches; batch_idx++) {
                    unsigned long long start_bucket = total_global_buckets * batch_idx;
                    unsigned long long end_bucket = (batch_idx == total_batches - 1) ? total_buckets : start_bucket + total_global_buckets;
                    unsigned long long buckets_in_batch = end_bucket - start_bucket;

                    // Create Read Tasks
                    if (batch_idx == 0) {
#pragma omp task depend(out : mergeBatches[batch_idx].readDone)
                        {
                            batch_read(batch_idx);
                        }
                    } else if (batch_idx >= max_active_batches) {
#pragma omp task depend(in : mergeBatches[batch_idx - 1].readDone, mergeBatches[batch_idx - max_active_batches].writeDone) depend(out : mergeBatches[batch_idx].readDone)
                        {
                            batch_read(batch_idx);
                        }
                    } else {
#pragma omp task depend(in : mergeBatches[batch_idx - 1].readDone) depend(out : mergeBatches[batch_idx].readDone)
                        {
                            batch_read(batch_idx);
                        }
                    }

                    // Create Merge Tasks
#pragma omp task depend(in : mergeBatches[batch_idx].readDone) depend(out : mergeBatches[batch_idx].mergeDone)
                    {
                        if (DEBUG) {
                            printf("[%d] Merge Started\n", batch_idx);
                        }

                        double start_time = omp_get_wtime();

                        MergeBatch* mergeBatch = &mergeBatches[batch_idx];
                        mergeBatch->mergedBuckets = (MemoTable2Record*)malloc(buckets_in_batch * global_bucket_size);

                        if (mergeBatch->mergedBuckets == NULL) {
                            fprintf(stderr, "Error: Failed to allocate memory for mergedBuckets (batch %d)\n", batch_idx);
                            exit(EXIT_FAILURE);
                        }

                        // FIXME: Find better way to utilize available threads for merging
                        // How many are available? Do we want to exhaust all of them?
                        int total_merge_threads = omp_get_num_threads() >= 16 ? omp_get_num_threads() - 32 : omp_get_num_threads() / 2;

#pragma omp taskloop
                        for (int merge_thread = 0; merge_thread < total_merge_threads; merge_thread++) {
                            int batch_size = floor((double)buckets_in_batch / total_merge_threads);
                            int merge_start_bucket = merge_thread * batch_size;
                            int merge_end_bucket = merge_thread == total_merge_threads - 1 ? buckets_in_batch : merge_start_bucket + batch_size;

                            for (unsigned long long bucket_idx = merge_start_bucket; bucket_idx < merge_end_bucket; bucket_idx++) {
                                for (int f = 0; f < TOTAL_FILES; f++) {
                                    memcpy(
                                        &mergeBatch->mergedBuckets[bucket_idx * records_per_global_bucket + f * num_records_in_bucket],
                                        &mergeBatch->buffer[f * buckets_in_batch * num_records_in_bucket + bucket_idx * num_records_in_bucket],
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
                            printf("[%d] Merge Complete: %.2f\n", batch_idx, elapsed);
                        }
                    }

                    // Create Write Tasks
                    if (batch_idx == 0) {
#pragma omp task depend(in : mergeBatches[batch_idx].mergeDone) depend(out : mergeBatches[batch_idx].writeDone)
                        {
                            batch_write(batch_idx);
                        }
                    } else {
#pragma omp task depend(in : mergeBatches[batch_idx].mergeDone, mergeBatches[batch_idx - 1].writeDone) depend(out : mergeBatches[batch_idx].writeDone)
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
        ssize_t res = write(merge_fd, (char*)plotData + bytes_written, total_bytes - bytes_written);
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

    if (merge_total_time == 0) {
        merge_total_time = process_time - read_total_time - write_total_time;
    }

    printf("\n\n");
    printf("[%.2fs] Completed merging %d K%d-files of total size %.2fGB\n", process_time, TOTAL_FILES, K, (double)size / (1024 * 1024 * 1024));
    printf("Merge Approach: %s\n", MERGE_APPROACH == 1 ? "Serial" : "Pipelined");
    printf("Source: %s\n", SOURCE);
    printf("Destination: %s\n", DESTINATION);
    printf("Read Time: %.2fs\n", read_total_time);
    printf("Write Time: %.2fs\n", write_total_time);
    printf("Merge Time: %.2fs\n\n\n", merge_total_time);

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

            FILE* fd = fopen(merge_filename, "rb");
            if (!fd) {
                perror("Error opening file");
            }

            MemoTable2Record* buffer = calloc(BATCH_SIZE * records_per_global_bucket, sizeof(MemoTable2Record));

            size_t offset = i * global_bucket_size;

            fseek(fd, offset, SEEK_SET);
            size_t elements_written = fread(buffer, sizeof(MemoTable2Record), BATCH_SIZE * records_per_global_bucket, fd);

            if (elements_written != BATCH_SIZE * records_per_global_bucket) {
                fprintf(stderr, "Error writing bucket to file");
                fclose(fd);
                exit(EXIT_FAILURE);
            }

            size_t end = i + BATCH_SIZE;
            if (end > total_buckets) {
                end = total_buckets;
            }

            MemoTable2Record* record;
            uint8_t hash[HASH_SIZE];

            for (unsigned long long j = i; j < end; j++) {
                bool flag = true;

                for (int k = 0; k < records_per_global_bucket; k++) {
                    record = &buffer[(j - i) * records_per_global_bucket + k];

                    if (byteArrayToLongLong(record->nonce1, NONCE_SIZE) != 0 || byteArrayToLongLong(record->nonce2, NONCE_SIZE) != 0) {
                        generateBlake3Pair(record->nonce1, record->nonce2, plotData[k / num_records_in_bucket].key, hash);

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

        printf("Total Buckets: %llu\nVerified Buckets: %llu\n", total_buckets, verified_global_buckets);
    }
}
