#include "vaultx.h"

int main(int argc, char* argv[]) {
    // Default values
    const char* approach = "for"; // Default approach
    int num_threads_io = 0;
    total_nonces = 1ULL << K; // 2^K iterations
    bool MERGE = false;
    char* SEARCH_STRING = NULL;
    int LOOKUP_COUNT = 1;

    // Define long options
    static struct option long_options[] = {
        { "approach", required_argument, 0, 'a' },
        { "threads", required_argument, 0, 't' },
        { "threads_io", required_argument, 0, 'i' },
        { "exponent", required_argument, 0, 'K' },
        { "size of merge batch", required_argument, 0, 'm' },
        { "memory limit", required_argument, 0, 'r' },
        { "batch_size", required_argument, 0, 'b' },
        { "memory_write", required_argument, 0, 'w' },
        { "circular_array", required_argument, 0, 'c' },
        { "verify", required_argument, 0, 'v' },
        { "Lookup S hashes", required_argument, 0, 'S' },
        { "benchmark", required_argument, 0, 'x' },
        { "full_buckets", required_argument, 0, 'y' },
        { "debug", required_argument, 0, 'd' },
        { "help", no_argument, 0, 'h' },
        { "total_files", required_argument, 0, 'n' },
        { "merge", required_argument, 0, 'M' },
        { "Merge File destination", required_argument, 0, 'T' },
        { "Small files source", required_argument, 0, 'F' },
        { 0, 0, 0, 0 }
    };

    int opt;
    int option_index = 0;

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:b:w:c:v:s:S:x:y:d:h:n:M:T:F:r:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'a':
            if (strcmp(optarg, "xtask") == 0 || strcmp(optarg, "task") == 0 || strcmp(optarg, "for") == 0 || strcmp(optarg, "tbb") == 0) {
                approach = optarg;
            } else {
                fprintf(stderr, "Invalid approach: %s\n", optarg);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            num_threads = atoi(optarg);
            if (num_threads <= 0) {
                fprintf(stderr, "Number of threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            num_threads_io = atoi(optarg);
            if (num_threads_io <= 0) {
                fprintf(stderr, "Number of I/O threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'K':
            K = atoi(optarg);
            if (K < 24 || K > 40) { // Limiting K to avoid overflow
                fprintf(stderr, "Exponent K must be between 24 and 40.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            total_nonces = 1ULL << K; // Compute 2^K
            break;
        case 'm':
            BATCH_MEMORY_MB = atoi(optarg);
            if (BATCH_MEMORY_MB <= 0) {
                fprintf(stderr, "Memory size must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'r':
            MEMORY_LIMIT_MB = atoi(optarg);
            if (MEMORY_LIMIT_MB <= 0) {
                fprintf(stderr, "Memory size must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'b':
            BATCH_SIZE = atoi(optarg);
            if (BATCH_SIZE < 1) {
                fprintf(stderr, "BATCH_SIZE must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'w':
            if (strcmp(optarg, "true") == 0) {
                MEMORY_WRITE = true;
            } else {
                MEMORY_WRITE = false;
            }
            break;
        case 'c':
            if (strcmp(optarg, "true") == 0) {
                CIRCULAR_ARRAY = true;
            } else {
                CIRCULAR_ARRAY = false;
            }
            break;
        case 'v':
            if (strcmp(optarg, "true") == 0) {
                VERIFY = true;
            } else {
                VERIFY = false;
            }
            break;
        case 'S':
            SEARCH_BATCH = true;
            SEARCH = true;
            HASHGEN = false;
            LOOKUP_COUNT = atoi(optarg);
            if (LOOKUP_COUNT < 1) {
                fprintf(stderr, "LOOKUP_COUNT must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'x':
            if (strcmp(optarg, "true") == 0) {
                BENCHMARK = true;
            } else {
                BENCHMARK = false;
            }
            break;
        case 'y':
            if (strcmp(optarg, "true") == 0) {
                FULL_BUCKETS = true;
            } else {
                FULL_BUCKETS = false;
            }
            break;
        case 'd':
            if (strcmp(optarg, "true") == 0) {
                DEBUG = true;
            } else {
                DEBUG = false;
            }
            break;
        case 'n':
            TOTAL_FILES = atoi(optarg);
            if (TOTAL_FILES < 1) {
                fprintf(stderr, "Number of files must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'M':
            MERGE_APPROACH = atoi(optarg);
            MERGE = true;
            HASHGEN = false;
            break;
        case 'T':
            DESTINATION = optarg;
            break;
        case 'F':
            SOURCE = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    // Set the number of threads if specified
    if (num_threads > 0) {
        omp_set_num_threads(num_threads);
    }

    if (num_threads_io == 0) {
        num_threads_io = 1;
    }

    // Display selected configurations
    if (BENCHMARK) {
        if (!SEARCH) {
            printf("Selected Approach           : %s\n", approach);
            printf("Number of Threads           : %d\n", num_threads > 0 ? num_threads : omp_get_max_threads());
            printf("Number of Threads I/O       : %d\n", num_threads_io > 0 ? num_threads_io : omp_get_max_threads());
            printf("Exponent K                  : %d\n", K);
        }
    }

    unsigned long long file_size_bytes = total_nonces * NONCE_SIZE;
    double file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);

    total_buckets = 1ULL << (PREFIX_SIZE * 8);

    num_records_in_bucket = total_nonces / total_buckets;

    num_records_in_shuffled_bucket = num_records_in_bucket * rounds;

    // FIXME:
    if (BENCHMARK) {
        if (SEARCH) {
            printf("SEARCH                      : true\n");
        } else {
            printf("Table1 File Size (GB)       : %.2f\n", file_size_gb);
            printf("Table1 File Size (bytes)    : %llu\n", file_size_bytes);

            printf("Table2 File Size (GB)       : %.2f\n", file_size_gb * 2);
            printf("Table2 File Size (bytes)    : %llu\n", file_size_bytes * 2);

            printf("Memory Size (MB)            : %llu\n", BATCH_MEMORY_MB);
            // printf("Memory Size (bytes)         : %llu\n", MEMORY_SIZE_bytes);

            // printf("Number of Hashes (RAM)      : %llu\n", num_records_per_round);

            printf("Number of Hashes (Disk)     : %llu\n", total_nonces);
            printf("Size of MemoRecord          : %lu\n", sizeof(MemoRecord));
            printf("Rounds                      : %llu\n", rounds);

            printf("Number of Buckets           : %llu\n", total_buckets);
            printf("Number of Records in Bucket : %llu\n", num_records_in_bucket);

            printf("BATCH_SIZE                  : %zu\n", BATCH_SIZE);

            if (HASHGEN)
                printf("HASHGEN                     : true\n");
            else
                printf("HASHGEN                     : false\n");

            if (MEMORY_WRITE)
                printf("MEMORY_WRITE                : true\n");
            else
                printf("MEMORY_WRITE                : false\n");

            if (CIRCULAR_ARRAY)
                printf("CIRCULAR_ARRAY              : true\n");
            else
                printf("CIRCULAR_ARRAY              : false\n");

            if (FULL_BUCKETS)
                printf("FULL_BUCKETS                : true\n");
            else
                printf("FULL_BUCKETS                : false\n");

            if (writeData) {
                // printf("Temporary File              : %s\n", FILENAME);
            }
        }
    }

    // Generate plots
    if (HASHGEN) {
        // Reset plots folder
        char* FOLDER[256];
        snprintf(FOLDER, sizeof(FOLDER), "/%s/arnav/vaultx/plots/", SOURCE);
        ensure_folder_exists(FOLDER);
        delete_contents(FOLDER);

        // Allocate memory
        buckets = (Bucket*)calloc(total_buckets, sizeof(Bucket));
        buckets_table2 = (BucketTable2*)calloc(total_buckets, sizeof(BucketTable2));
        table2 = (MemoTable2Record*)calloc(total_nonces, sizeof(MemoTable2Record));

        if (buckets == NULL || buckets_table2 == NULL || table2 == NULL) {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        for (unsigned long long i = 0; i < total_buckets; i++) {
            buckets[i].records = (MemoRecord*)calloc(num_records_in_bucket, sizeof(MemoRecord));
            if (buckets[i].records == NULL) {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
                exit(EXIT_FAILURE);
            }
        }

        double total_time = 0;
        double total_gen_time = 0.0;
        double total_io_time = 0.0;

        for (int f = 1; f <= TOTAL_FILES; f++) {
            double file_time = 0;

            uint8_t plot_id[32];
            generate_plot_id(plot_id);
            derive_key(K, plot_id, key);

            double hashgen_start_time = omp_get_wtime();

            // Reset bucket counts
            for (unsigned long long i = 0; i < total_buckets; i++) {
                buckets[i].count = 0;
                buckets[i].count_waste = 0;
                buckets[i].full = false;
                buckets[i].flush = 0;

                buckets_table2[i].count = 0;
                buckets_table2[i].count_waste = 0;
                buckets_table2[i].full = false;
                buckets_table2[i].flush = 0;
            }

            // TODO: Alternatives: Recursive task based parallelism and Tasking Model Approach

            // Parallel For Loop Approach
            if (strcmp(approach, "for") == 0) {
                generateHashes();
            }

            // Table1: Hash Generation Complete
            double hashgen_end_time = omp_get_wtime();
            double hashgen_time = hashgen_end_time - hashgen_start_time;
            total_gen_time += hashgen_time;
            file_time += hashgen_time;

            printf("[File %d] %-40s: %.2fs\n", f, "Table1: Hash Generation Complete", hashgen_time);

            // Calculate Table1 storage efficiency
            if (DEBUG) {
                unsigned long long nonces_generated = 0;

#pragma omp parallel for reduction(+ : nonces_generated)
                for (unsigned long long i = 0; i < total_buckets; i++) {
                    nonces_generated += buckets[i].count;
                }

                printf("[File %d] %-40s: %.2f%%\n", current_file, "Table 1 Storage Efficiency", 100 * ((double)nonces_generated / total_nonces));
            }

            // TODO: Verify records in Table1

            // Table2 Generation: Find Matches from Table1
            double matching_start_time = omp_get_wtime();

            findMatches();

            double matching_end_time = omp_get_wtime();
            double matching_time = matching_end_time - matching_start_time;
            file_time += matching_time;
            total_gen_time += matching_time;

            printf("[File %d] %-40s: %.2fs\n", current_file, "Table2: Matching Complete", matching_time);

            if (DEBUG) {
                unsigned long long nonces_generated = 0;

#pragma omp parallel for reduction(+ : nonces_generated)
                for (unsigned long long i = 0; i < total_buckets; i++) {
                    nonces_generated += buckets_table2[i].count;
                }

                printf("[File %d] %-40s: %.2f%%\n", current_file, "Table 2 Storage Efficiency", 100 * ((double)nonces_generated / total_nonces));
            }

            // TODO: Verify records in Table2

            // Write Table 2 to disk
            double fileio_start_time = omp_get_wtime();

            writeTable2(plot_id);

            double fileio_end_time = omp_get_wtime();
            double fileio_time = fileio_end_time - fileio_start_time;
            file_time += fileio_time;
            total_io_time += fileio_time;

            printf("[File %d] %-40s: %.2fs\n", current_file, "Finished writing to disk", fileio_time);

            // Clearing all buckets from memory
            if (current_file < TOTAL_FILES) {
                double empty_start_time = omp_get_wtime();

                unsigned long long buckets_per_thread = total_buckets / num_threads;
                unsigned long long buckets_remainder = total_buckets % num_threads;

                unsigned long long table2_bytes = total_nonces * sizeof(MemoTable2Record);
                unsigned long long chunk_size = table2_bytes / num_threads;
                unsigned long long chunk_remainder = table2_bytes % num_threads;

#pragma omp parallel num_threads(num_threads)
                {
                    int tid = omp_get_thread_num();

                    // Clear Table1
                    unsigned long long b_start = tid * buckets_per_thread + (tid < buckets_remainder ? tid : buckets_remainder);
                    unsigned long long b_count = buckets_per_thread + (tid < buckets_remainder ? 1 : 0);
                    unsigned long long b_end = b_start + b_count;

                    for (unsigned long long i = b_start; i < b_end; i++) {
                        memset(buckets[i].records, 0, sizeof(MemoRecord) * num_records_in_bucket);
                    }

                    // Clear Table2
                    unsigned long long t_offset = tid * chunk_size;
                    unsigned long long t_size = (tid == num_threads - 1) ? (chunk_size + chunk_remainder) : chunk_size;
                    memset((char*)table2 + t_offset, 0, t_size);
                }

                double empty_end_time = omp_get_wtime();
                double empty_time = empty_end_time - empty_start_time;
                file_time += empty_time;

                printf("[File %d] %-40s: %.2fs\n", current_file, "Cleared Memory", empty_time);
            }

            printf("File %d   %-40s: %.2fs\n\n\n", current_file, "Processing Complete", file_time);

            total_time += file_time;

            // Prepare for next file
            current_file++;
            full_buckets_global = 0;
        }

        double sync_start_time = omp_get_wtime();

        printf("Syncing. . . \n");
        sync();

        double sync_time = omp_get_wtime() - sync_start_time;
        total_time += sync_time;
        total_io_time += sync_time;

        printf("[%.2fs] Completed generating %d files\n", total_time, TOTAL_FILES);

        // TODO: Throughput calculation
        double throughput_hash = (TOTAL_FILES * total_nonces * 2 / (total_gen_time)) / (1e6);
        double throughput_io = (TOTAL_FILES * total_nonces * sizeof(MemoTable2Record) / (total_io_time)) / (1024 * 1024);
        printf("Hashgen Throughput: %.2f MH/s\nIO Throughput: %.2f MB/s\n", throughput_hash, throughput_io);
    }

    // TODO: PERF to see bottlenecks

    // TODO: Analzye distribution in L1,L2 cache

    // TODO: Try taskloop pragma (CHATGPT history)

    // TODO: Small tests on data/fast2

    // TODO: Optimizations in makefile

    // TODO: Play around with scheduling

    // TODO: pread and pwrite, no merge required?

    // TODO: Theoretical peak: poster idea

    if (MERGE) {
        merge();
    }

    if (SEARCH) {
        double small_search_time = 0.0;
        double merge_search_time = 0.0;

        unsigned long long bucketSize = num_records_in_bucket * sizeof(MemoTable2Record);
        unsigned long long global_bucket_size = num_records_in_bucket * TOTAL_FILES * sizeof(MemoTable2Record);

        // Initialize small plots
        double small_setup_start_time = omp_get_wtime();

        PlotData plotData[TOTAL_FILES];
        int fds[TOTAL_FILES];
        int counter = 0;

        char dir_name[256];
        snprintf(dir_name, sizeof(dir_name), "/%s/arnav/vaultx/plots", SOURCE);

        DIR* d = opendir(dir_name);
        if (!d) {
            perror("opendir");
            return 1;
        }

        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && counter < TOTAL_FILES) {
            if (dir->d_name[0] == '.')
                continue;

            int kVal = 0;
            char hex[129];
            if (sscanf(dir->d_name, "K%d_%128[^.].plot", &kVal, hex) != 2) {
                printf("Parsing failed for %s\n", dir->d_name);
                continue;
            }

            plotData[counter].K = kVal;
            uint8_t* plot_id = hexStringToByteArray(hex);
            derive_key(kVal, plot_id, plotData[counter].key);
            free(plot_id);

            char fullpath[256];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_name, dir->d_name);

            int fd = open(fullpath, O_RDONLY);
            if (fd < 0) {
                perror("Small plot open");
                continue;
            }
            fds[counter] = fd;

            counter++;
        }

        closedir(d);

        small_search_time += omp_get_wtime() - small_setup_start_time;

        // Initialize merged plot
        double merge_setup_start_time = omp_get_wtime();

        char merge_filename[100];
        snprintf(merge_filename, sizeof(merge_filename), "/%s/arnav/vaultx/merge_%d_%d.plot", DESTINATION, K, TOTAL_FILES);
        int merge_fd = open(merge_filename, O_RDWR | O_CREAT | O_APPEND, 0644);

        if (merge_fd == -1) {
            perror("Error opening merged plot");
            return EXIT_FAILURE;
        }

        PlotData plotData_merge[TOTAL_FILES];

        off_t offset = total_nonces * sizeof(MemoTable2Record) * TOTAL_FILES;
        ssize_t bytes_to_read = sizeof(PlotData) * TOTAL_FILES;

        ssize_t bytes_read = pread(merge_fd, plotData_merge, bytes_to_read, offset);
        if (bytes_read < 0) {
            perror("pread on merged plotdata failed");
            close(merge_fd);
            return 1;
        }
        if (bytes_read != bytes_to_read) {
            fprintf(stderr, "Incomplete read: expected %zd bytes, got %zd bytes\n", bytes_to_read, bytes_read);
            close(merge_fd);
            return 1;
        }

        merge_search_time += omp_get_wtime() - merge_setup_start_time;

        // Perform lookups
        for (int s = 0; s < LOOKUP_COUNT; s++) {
            uint8_t* targetHash = getRandomHash(HASH_SIZE);
            if (!targetHash) {
                fprintf(stderr, "Failed to allocate targetHash\n");
                continue;
            }

            int bucketIndex = getBucketIndex(targetHash);

            // Lookup in smaller plots
            double small_start_time = omp_get_wtime();

            for (int idx = 0; idx < TOTAL_FILES; idx++) {
                MemoTable2Record bucket[num_records_in_bucket];
                off_t offset = bucketIndex * bucketSize;

                ssize_t bytes_read = pread(fds[idx], bucket, bucketSize, offset);
                if (bytes_read < 0) {
                    perror("pread");
                    continue;
                }

                uint8_t hash[HASH_SIZE];
                for (int i = 0; i < num_records_in_bucket; i++) {
                    if (byteArrayToLongLong(bucket[i].nonce1, NONCE_SIZE) != 0 || byteArrayToLongLong(bucket[i].nonce2, NONCE_SIZE) != 0) {
                        generateBlake3Pair(bucket[i].nonce1, bucket[i].nonce2, plotData[idx].key, hash);
                        if (memcmp(hash, targetHash, HASH_SIZE) == 0) {
                            printf("Hash Match Found: %s, Nonce1: %llu, Nonce2: %llu, Key: %s\n",
                                byteArrayToHexString(hash, HASH_SIZE),
                                byteArrayToLongLong(bucket[i].nonce1, NONCE_SIZE),
                                byteArrayToLongLong(bucket[i].nonce2, NONCE_SIZE),
                                byteArrayToHexString(plotData[idx].key, 32));
                        }
                    }
                }
            }

            small_search_time += omp_get_wtime() - small_start_time;

            // Lookup in merged plot
            double merge_start_time = omp_get_wtime();

            MemoTable2Record global_bucket[num_records_in_bucket * TOTAL_FILES];
            off_t offset = bucketIndex * global_bucket_size;

            ssize_t bytes_read = pread(merge_fd, global_bucket, global_bucket_size, offset);
            if (bytes_read < 0) {
                perror("pread failed");
                close(merge_fd);
                return 1;
            }
            if ((size_t)bytes_read != global_bucket_size) {
                fprintf(stderr, "pread incomplete: expected %zu bytes, got %zd bytes\n", global_bucket_size, bytes_read);
                close(merge_fd);
                return 1;
            }

            uint8_t hash[HASH_SIZE];

            for (unsigned long long i = 0; i < num_records_in_bucket * TOTAL_FILES; i++) {
                if (byteArrayToLongLong(global_bucket[i].nonce1, NONCE_SIZE) != 0 || byteArrayToLongLong(global_bucket[i].nonce2, NONCE_SIZE) != 0) {
                    int plotIndex = i / num_records_in_bucket;
                    generateBlake3Pair(global_bucket[i].nonce1, global_bucket[i].nonce2, plotData_merge[plotIndex].key, hash);
                    if (memcmp(hash, targetHash, HASH_SIZE) == 0) {
                        printf("Hash Match Found: %s, Nonce1: %llu, Nonce2: %llu, Key: %s\n",
                            byteArrayToHexString(hash, HASH_SIZE),
                            byteArrayToLongLong(global_bucket[i].nonce1, NONCE_SIZE),
                            byteArrayToLongLong(global_bucket[i].nonce2, NONCE_SIZE),
                            byteArrayToHexString(plotData[plotIndex].key, 32));
                    }
                }
            }

            merge_search_time += omp_get_wtime() - merge_start_time;

            int ret = system("sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'");

            if (ret != 0) {
                perror("Failed to drop caches");
                return 1;
            }
        }

        // FIXME: Deletes all plots
        // Cleanup all open files
        for (int i = 0; i < TOTAL_FILES; i++) {
            close(fds[i]);
            close(merge_fd);
            remove(merge_filename);
            delete_contents(dir_name);
            ensure_folder_exists(dir_name);
        }

        printf("Small Plots Lookup Time: %.6f\n", small_search_time);
        printf("Merged Plot Lookup Time: %.6f\n", merge_search_time);
    }

    return 0;
}
