#include "vaultx.h"

// Function to display usage information
void print_usage(char* prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -a, --approach [xtask|task|for|tbb]   Select parallelization approach (default: for)\n");
    printf("  -t, --threads NUM                     Number of threads to use (default: number of available cores)\n");
    printf("  -i, --threads_io NUM                  Number of I/O threads (default: number of available cores)\n");
    printf("  -K, --exponent NUM                    Exponent K to compute 2^K number of records (default: 4)\n");
    printf("  -m, --memory NUM                      Memory size in MB (default: 1)\n");
    printf("  -b, --batch-size NUM                  Batch size (default: 1024)\n");
    printf("  -s, --search STRING                   Search for a specific hash prefix in the file\n");
    printf("  -S, --search-batch NUM                Search for a specific hash prefix in the file in batch mode\n");
    printf("  -x, --benchmark                       Enable benchmark mode (default: false)\n");
    printf("  -h, --help                            Display this help message\n");
    printf("  -n, --total_files NUM                 Number of K fiels to generate\n");
    printf("  -M, --merge                           Merge k files into a big file\n");
    printf("\nExample:\n");
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo\n", prog_name);
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -x true (Only prints time)\n", prog_name);
    printf("  %s -a for -t 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -2 true\n", prog_name);
}

// Function to compute the bucket index based on hash prefix
// FIXME: Same problem of little endian big endian
off_t getBucketIndex(const uint8_t* hash) {
    off_t index = 0;
    for (size_t i = 0; i < PREFIX_SIZE && i < HASH_SIZE; i++) {
        index = (index << 8) | hash[i];
    }
    return index;
}

// Function to convert bytes to unsigned long long
// FIXME: This has unexpected behavior. It's assuming byte array is in big-endian format
// But works for our use case of comparing distance with expected distance
unsigned long long byteArrayToLongLong(const uint8_t* byteArray, size_t length) {
    unsigned long long result = 0;
    for (size_t i = 0; i < length; ++i) {
        result = (result << 8) | (unsigned long long)byteArray[i];
    }
    return result;
}

// Function to check if a nonce is non-zero
bool is_nonce_nonzero(const uint8_t* nonce, size_t nonce_size) {
    // Check for NULL pointer
    if (nonce == NULL) {
        // Handle error as needed
        return false;
    }

    // Iterate over each byte of the nonce
    for (size_t i = 0; i < nonce_size; ++i) {
        if (nonce[i] != 0) {
            // Found a non-zero byte
            return true;
        }
    }

    // All bytes are zero
    return false;
}

uint8_t* hexStringToByteArray(const char* hexString) {
    size_t hexLen = strlen(hexString);
    uint8_t* byteArray = (uint8_t*)malloc(hexLen * sizeof(uint8_t));
    // size_t hexLen = strlen(hexString);
    if (hexLen % 2 != 0) {
        return NULL; // Error: Invalid hexadecimal string length
    }

    size_t byteLen = hexLen / 2;
    size_t byteArraySize = byteLen;
    if (byteLen > byteArraySize) {
        return NULL; // Error: Byte array too small
    }

    for (size_t i = 0; i < byteLen; ++i) {
        if (sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]) != 1) {
            return NULL; // Error: Failed to parse hexadecimal string
        }
    }

    return byteArray;
}

uint64_t largest_power_of_two_less_than(uint64_t number) {
    if (number == 0) {
        return 0;
    }

    // Decrement number to handle cases where number is already a power of 2
    number--;

    // Set all bits to the right of the most significant bit
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number |= number >> 32; // Only needed for 64-bit integers

    // The most significant bit is now set; shift right to get the largest power of 2 less than the original number
    return (number + 1) >> 1;
}

char* byteArrayToHexString(const unsigned char* bytes, size_t len) {
    char* out = malloc(len * 2 + 1); // 2 hex chars per byte + null terminator
    if (out == NULL) {
        return NULL; // Allocation failed
    }

    for (size_t i = 0; i < len; i++) {
        sprintf(out + (i * 2), "%02x", bytes[i]);
    }
    out[len * 2] = '\0'; // Null-terminate
    return out;
}

char* num_to_hex(unsigned long long num, size_t total_bytes) {
    size_t hex_len = total_bytes * 2;

    // Allocate space for hex digits + null terminator
    char* hexstr = malloc(hex_len + 1);
    if (hexstr == NULL)
        return NULL;

    // Format the number with leading zeroes to ensure fixed width
    snprintf(hexstr, hex_len + 1, "%0*llx", (int)hex_len, num);

    return hexstr;
}

void print_bits(const uint8_t* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            putchar((arr[i] & (1 << bit)) ? '1' : '0');
        }
        printf(" "); // optional space between bytes
    }
    printf("\n");
}

void print_number_binary_bytes(uint64_t number, size_t size) {
    const unsigned char* bytes = (const unsigned char*)&number;

    printf("Binary (little endian, %zu bytes):\n", size);

    for (size_t i = 0; i < size; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            putchar((bytes[i] >> bit) & 1 ? '1' : '0');
        }
        printf(" ");
    }

    printf("\n");
}

void delete_contents(const char* folder_path) {
    DIR* dir = opendir(folder_path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent* entry;
    char path[1024];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively delete subdirectory
                delete_contents(path);
                if (rmdir(path) != 0)
                    perror("rmdir failed");
            } else {
                if (remove(path) != 0)
                    perror("remove failed");
            }
        }
    }

    closedir(dir);
}

void ensure_folder_exists(const char* path) {
    struct stat st = { 0 };

    if (stat(path, &st) == -1) {
        // Folder doesn't exist, try to create it
        if (mkdir(path, 0777) != 0) {
            perror("mkdir failed");
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s exists but is not a directory\n", path);
    }
}

int main(int argc, char* argv[]) {
    // printf("size of MemoTable2Record: %zu\n", sizeof(MemoTable2Record));
    // printf("size of MemoRecord2: %zu\n", sizeof(MemoRecord2));
    // Default values
    const char* approach = "for"; // Default approach
    int num_threads = 0; // 0 means OpenMP chooses
    int num_threads_io = 0;
    total_nonces = 1ULL << K; // 2^K iterations
    unsigned long long MEMORY_SIZE_MB = 1;
    int TOTAL_FILES = 2;
    bool MERGE = false;
    char* SEARCH_STRING = NULL;

    // Define long options
    static struct option long_options[] = {
        { "approach", required_argument, 0, 'a' },
        { "threads", required_argument, 0, 't' },
        { "threads_io", required_argument, 0, 'i' },
        { "exponent", required_argument, 0, 'K' },
        { "memory", required_argument, 0, 'm' },
        { "batch_size", required_argument, 0, 'b' },
        { "memory_write", required_argument, 0, 'w' },
        { "circular_array", required_argument, 0, 'c' },
        { "verify", required_argument, 0, 'v' },
        { "search", required_argument, 0, 's' },
        { "prefix_search_size", required_argument, 0, 'S' },
        { "benchmark", required_argument, 0, 'x' },
        { "full_buckets", required_argument, 0, 'y' },
        { "debug", required_argument, 0, 'd' },
        { "help", no_argument, 0, 'h' },
        { "total_files", required_argument, 0, 'n' },
        { "merge", required_argument, 0, 'M' },
        { 0, 0, 0, 0 }
    };

    int opt;
    int option_index = 0;

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:b:w:c:v:s:S:x:y:d:h:n:M:", long_options, &option_index)) != -1) {
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
            MEMORY_SIZE_MB = atoi(optarg);
            if (MEMORY_SIZE_MB <= 0) {
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
        case 's':
            SEARCH_STRING = optarg;
            SEARCH = true;
            HASHGEN = false;
            break;
        case 'S':
            SEARCH_BATCH = true;
            SEARCH = true;
            HASHGEN = false;
            PREFIX_SEARCH_SIZE = atoi(optarg);
            if (PREFIX_SEARCH_SIZE < 1) {
                fprintf(stderr, "PREFIX_SEARCH_SIZE must be 1 or greater.\n");
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

    if (BENCHMARK) {
        if (SEARCH) {
            printf("SEARCH                      : true\n");
        } else {
            printf("Table1 File Size (GB)       : %.2f\n", file_size_gb);
            printf("Table1 File Size (bytes)    : %llu\n", file_size_bytes);

            printf("Table2 File Size (GB)       : %.2f\n", file_size_gb * 2);
            printf("Table2 File Size (bytes)    : %llu\n", file_size_bytes * 2);

            printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
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

            printf("[File %d] %-40s: %.2fs\n", current_file, "Finished writing to disk", fileio_time);

            // Clearing all buckets from memory
            if (current_file < TOTAL_FILES) {
                double empty_start_time = omp_get_wtime();

                for (unsigned long long i = 0; i < total_buckets; i++) {
                    memset(buckets[i].records, 0, sizeof(MemoRecord) * num_records_in_bucket);
                }

                double empty_end_time = omp_get_wtime();
                double empty_time = empty_end_time - empty_start_time;
                file_time += empty_time;
            }

            printf("File %d   %-40s: %.2fs\n\n\n", current_file, "Processing Complete", file_time);

            total_time += file_time;

            // Prepare for next file
            current_file++;
            full_buckets_global = 0;

            // TODO: Throughput calculation
            // throughput_hash = (num_records_per_round / (hashgen_time + elapsed_time_io)) / (1e6);
            // throughput_io = (num_records_per_round * sizeof(MemoTable2Record)) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);
            // printf("[%.2f] HashGen %.2f%%: %.2f MH/s : I/O %.2f MB/s\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds, throughput_hash, throughput_io);
        }

        printf("[%.2fs] Completed generating %d files\n", total_time, TOTAL_FILES);
    }

    // TODO: PERF to see bottlenecks

    // TODO: Analzye distribution in L1,L2 cache

    // TODO: Try taskloop pragma (CHATGPT history)

    // TODO: Figure out optimal batch size and thread number

    // TODO: Small tests on data/fast2

    // TODO: Data-l parallel logic works pretty well

    // TODO: Optimizations in makefile

    // TODO: Try new merging approaches

    // TODO: Play around with scheduling

    // TODO: pread and pwrite, no merge required?

    // TODO: Theoretical peak: poster idea

    if (MERGE) {
        int MAX_FILENAME_LEN = 100;
        char filenames[TOTAL_FILES][MAX_FILENAME_LEN];

        DIR* d = opendir("plots");
        struct dirent* dir;

        int count = 0;

        if (d) {
            while ((dir = readdir(d)) != NULL && count < TOTAL_FILES) {
                if (dir->d_name[0] != '.') { // skip hidden files
                    // Build full path "plots/filename"
                    snprintf(filenames[count], MAX_FILENAME_LEN, "plots/%s", dir->d_name);
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

            char buffer[256];
            strncpy(buffer, filename, sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            // Remove ".plot" extension
            char* dot = strrchr(buffer, '.');
            if (dot != NULL) {
                *dot = '\0';
            }

            // Skip the "plots/" prefix
            char* base = buffer;
            if (strncmp(buffer, "plots/", 6) == 0) {
                base = buffer + 6;
            }

            // Now split at '_'
            int kVal = atoi(strtok(base, "_") + 1);
            char* hexPart = strtok(NULL, "_");

            uint8_t* plot_id = hexStringToByteArray(hexPart);

            plotData[i].K = kVal;
            derive_key(kVal, plot_id, plotData[i].key);
        }

        //   closedir(dir);

        printf("Memory Size: %lluMB\n", MEMORY_SIZE_MB);
        printf("Threads: %d\n", num_threads);
        printf("Files: %d\n\n\n", TOTAL_FILES);

        unsigned long long records_per_global_bucket = TOTAL_FILES * num_records_in_bucket;
        unsigned long long global_bucket_size = records_per_global_bucket * sizeof(MemoTable2Record);

        // Number of global buckets that can be stored in memory
        unsigned long long total_global_buckets = (unsigned long long)floor(MEMORY_SIZE_MB * 1024 * 1024 / global_bucket_size);

        // Allocate memory
        MemoTable2Record* mergedBuckets = (MemoTable2Record*)calloc(total_global_buckets * records_per_global_bucket, sizeof(MemoTable2Record));

        FileRecords file_records[TOTAL_FILES];

        for (int i = 0; i < TOTAL_FILES; i++) {
            file_records[i].records = (MemoTable2Record*)calloc(total_global_buckets * num_records_in_bucket, sizeof(MemoTable2Record));
        }

        if (mergedBuckets == NULL) {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        // int cpu = sched_getcpu();
        // printf("Running on CPU: %d\n", cpu);

        // Merge file
        char merge_filename[100];
        // snprintf(merge_filename, sizeof(merge_filename), "/data-l/arnav/vaultx/merge_%d_%d.plot", K, TOTAL_FILES);
        snprintf(merge_filename, sizeof(merge_filename), "merge_%d_%d.plot", K, TOTAL_FILES);

        remove(merge_filename);

        int merge_fd = open(merge_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

        if (merge_fd == -1) {
            perror("Error opening file");
            return EXIT_FAILURE;
        }

        // Preload Files
        FILE* files[TOTAL_FILES];

        for (int i = 0; i < TOTAL_FILES; i++) {
            char* filename = filenames[i];

            files[i] = fopen(filename, "rb");
            if (!files[i]) {
                perror("Error opening file");
                // return 1;
            }
        }

        unsigned long long end;
        double batch_start_time, merge_start_time;
        double start_time = omp_get_wtime();
        double read_total_time = 0;
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

                for (unsigned long long i = 0; i < total_buckets; i += total_global_buckets) {

#pragma omp single

                    {
                        end = i + total_global_buckets;
                        if (end > total_buckets) {
                            end = total_buckets;
                        }

                        batch_start_time = omp_get_wtime();

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
                        }

                        double read_time = omp_get_wtime() - batch_start_time;
                        read_total_time += read_time;

                        double throughput_MBps = MEMORY_SIZE_MB / read_time; // MB per second
                        printf("Read : %.4fs, Rate: %.2f MB/s\n", read_time, throughput_MBps);

                        merge_start_time = omp_get_wtime();
                    }

#pragma omp for
                    for (int f = 0; f < TOTAL_FILES; f++) {
                        FileRecords* file_record = &file_records[f];
                        unsigned long long off = f * num_records_in_bucket;
                        for (unsigned long long k = 0; k < end - i; k++) {
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

                        double throughput_MBps = MEMORY_SIZE_MB / write_time; // MB per second
                        printf("Write: %.4fs, Rate: %.2f MB/s\n", write_time, throughput_MBps);

                        printf("[%.2f%%] | Batch Time: %.6fs | Total Time: %.2fs\n\n", ((double)end / total_buckets) * 100, omp_get_wtime() - batch_start_time, omp_get_wtime() - start_time);
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

        // TODO: Glances
        // TODO: Graphs (parallelism (some for hdd 4 max)

        // TODO: Profiling

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

        double merge_total_time = omp_get_wtime() - start_time;

        printf("Merge complete: %.2fs\n", merge_total_time);
        printf("Read Time: %.2fs\n", read_total_time);
        printf("Write Time: %.2fs\n", write_total_time);
        printf("Merge Time: %.2fs\n\n\n", merge_total_time - read_total_time - write_total_time);

        for (int i = 0; i < TOTAL_FILES; i++) {
            fclose(files[i]);
        }

        // Verify complete merge plot
        //         if (VERIFY) {
        //             printf("Starting verification: \n");

        //             size_t verified_global_buckets = 0;

        // #pragma omp parallel for
        //             for (unsigned long long i = 0; i < total_buckets; i += BATCH_SIZE) {

        //                 FILE* fd = fopen(merge_filename, "rb");
        //                 if (!fd) {
        //                     perror("Error opening file");
        //                 }

        //                 MemoTable2Record* buffer = calloc(BATCH_SIZE * records_per_global_bucket, sizeof(MemoTable2Record));

        //                 size_t offset = i * global_bucket_size;

        //                 fseek(fd, offset, SEEK_SET);
        //                 size_t elements_written = fread(buffer, sizeof(MemoTable2Record), BATCH_SIZE * records_per_global_bucket, fd);

        //                 if (elements_written != BATCH_SIZE * records_per_global_bucket) {
        //                     fprintf(stderr, "Error writing bucket to file");
        //                     fclose(fd);
        //                     exit(EXIT_FAILURE);
        //                 }

        //                 size_t end = i + BATCH_SIZE;
        //                 if (end > total_buckets) {
        //                     end = total_buckets;
        //                 }

        //                 MemoTable2Record* record;
        //                 uint8_t fileId[FILEID_SIZE];
        //                 uint8_t hash[HASH_SIZE];

        //                 for (unsigned long long j = i; j < end; j++) {
        //                     bool flag = true;

        //                     for (int k = 0; k < records_per_global_bucket; k++) {
        //                         record = &buffer[(j - i) * records_per_global_bucket + k];

        //                         if (byteArrayToLongLong(record->nonce1, NONCE_SIZE) != 0 || byteArrayToLongLong(record->nonce2, NONCE_SIZE) != 0) {
        //                             generateBlake3Pair(record->nonce1, record->nonce2, plotData[k / num_records_in_bucket].key, hash);

        //                             if (byteArrayToLongLong(hash, PREFIX_SIZE) != j) {
        //                                 flag = false;
        //                                 break;
        //                             }
        //                         }
        //                     }

        //                     if (flag) {
        // #pragma omp atomic
        //                         verified_global_buckets++;
        //                     }
        //                 }

        //                 free(buffer);

        //                 fclose(fd);
        //             }

        //             printf("Total Buckets: %llu\nVerified Buckets: %llu\n", total_buckets, verified_global_buckets);
        //         }

        free(mergedBuckets);

        for (int i = 0; i < TOTAL_FILES; i++) {
            free(file_records[i].records);
        }
    }

    if (SEARCH) {
        double start_time = omp_get_wtime();

        printf("Searching: %s\n", SEARCH_STRING);

        int k, total_files;

        DIR* dir = opendir(".");
        FILE* fd;
        struct dirent* entry;

        if (!dir) {
            perror("opendir");
            return 1;
        }

        while ((entry = readdir(dir)) != NULL) {
            const char* filename = entry->d_name;

            // Check if filename starts with "merge_"
            if (strncmp(filename, "merge_", 6) != 0)
                continue;

            // Remove ".plot" from the end
            char temp[512];
            strncpy(temp, filename, sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';
            temp[strlen(temp) - 5] = '\0'; // Remove ".plot"

            // Extract val1 and val2
            if (sscanf(temp, "merge_%d_%d", &k, &total_files) == 2) {
                printf("Filename   : %s\n", filename);
                printf("K          : %d\n", k);
                printf("Total files: %d\n", total_files);
            }

            fd = fopen(filename, "rb");
            if (!fd) {
                perror("fopen failed");
                continue;
            }
        }

        closedir(dir);

        PlotData plotData[total_files];

        off_t offset = sizeof(PlotData) * total_files;

        if (fseek(fd, -offset, SEEK_END) != 0) {
            perror("fseek failed");
            fclose(fd);
            return 1;
        }

        size_t nread = fread(plotData, sizeof(PlotData), total_files, fd);
        if ((int)nread != total_files) {
            fprintf(stderr, "fread failed: expected %d elements, got %zu\n", total_files, nread);
            fclose(fd);
            return 1;
        }

        const char* start = SEARCH_STRING;
        while (*start && !isdigit((unsigned char)*start)) {
            start++;
        }

        unsigned long long bucketIndex = byteArrayToLongLong(hexStringToByteArray((SEARCH_STRING)), PREFIX_SIZE);
        printf("bucket: %llu\n", bucketIndex);

        // FIXME: Recompute all values and reuse those variables
        size_t off = bucketIndex * num_records_in_bucket * total_files * sizeof(MemoTable2Record);

        if (fseek(fd, off, SEEK_SET) != 0) {
            perror("fseek failed");
            fclose(fd);
            return 1;
        }

        MemoTable2Record bucket[num_records_in_bucket * total_files];

        size_t records_read = fread(bucket, sizeof(MemoTable2Record), num_records_in_bucket * total_files, fd);
        if (records_read != num_records_in_bucket * total_files) {
            fprintf(stderr, "fread failed: expected %d elements, got %zu\n", total_files, nread);
            fclose(fd);
            return 1;
        }

        uint8_t hash[HASH_SIZE];

        for (unsigned long long i = 0; i < num_records_in_bucket * total_files; i++) {
            if (byteArrayToLongLong(bucket[i].nonce1, NONCE_SIZE) != 0 || byteArrayToLongLong(bucket[i].nonce2, NONCE_SIZE) != 0) {
                generateBlake3Pair(bucket[i].nonce1, bucket[i].nonce2, plotData[i / num_records_in_bucket].key, hash);

                if (memcmp(hash, hexStringToByteArray(SEARCH_STRING), strlen(SEARCH_STRING) / 2) == 0) {
                    printf("Hash Match Found: %s, Nonce1: %llu, Nonce2: %llu, Key: %s\n", byteArrayToHexString(hash, HASH_SIZE), byteArrayToLongLong(bucket[i].nonce1, NONCE_SIZE), byteArrayToLongLong(bucket[i].nonce2, NONCE_SIZE), byteArrayToHexString(plotData[i / num_records_in_bucket].key, 32));
                }
            }
        }

        printf("Search Complete: %.4fs\n", omp_get_wtime() - start_time);
    }

    return 0;
}

// will need to check on MacOS with a spinning hdd if we need to call sync() to flush all filesystems
// #ifdef __linux__
//         if (writeData) {
//             if (DEBUG)
//                 printf("Final flush in progress...\n");
//             int fd2 = open(FILENAME_TABLE2, O_RDWR);
//             if (fd2 == -1) {
//                 printf("Error opening file %s (#6)\n", FILENAME_TABLE2);

//                 perror("Error opening file");
//                 return EXIT_FAILURE;
//             }

//             // Sync the entire filesystem
//             if (syncfs(fd2) == -1) {
//                 perror("Error syncing filesystem with syncfs");
//                 close(fd2);
//                 return EXIT_FAILURE;
//             }
//         }
// #endif

// Calculate total throughput
// double total_throughput = (total_nonces / elapsed_time) / 1e6;
// if (!BENCHMARK) {
//     printf("Total Throughput: %.2f MH/s  %.2f MB/s\n", total_throughput, total_throughput * NONCE_SIZE);
//     printf("Total Time: %.6f seconds\n", elapsed_time);
// } else {
//     // printf("%s %d %lu %d %llu %.2f %zu %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", approach, K, sizeof(MemoRecord), num_threads, MEMORY_SIZE_MB, file_size_gb, BATCH_SIZE, total_throughput, total_throughput * NONCE_SIZE, hashgen_total_time, elapsed_time_io_total, elapsed_time_io2_total, elapsed_time - elapsed_time_hash_total - elapsed_time_io_total - elapsed_time_io2_total, elapsed_time);
//     return 0;
// }
// end of HASHGEN

// omp_set_num_threads(num_threads);

// if (SEARCH && !SEARCH_BATCH) {
//     search_memo_records(FILENAME_TABLE2, SEARCH_STRING);
// }

// if (SEARCH_BATCH) {
//     search_memo_records_batch(FILENAME_TABLE2, BATCH_SIZE, PREFIX_SEARCH_SIZE);
// }

// Call the function to count zero-value MemoRecords
// printf("verifying efficiency of final stored file...\n");
// count_zero_memo_records(FILENAME_FINAL);

// Call the function to process MemoRecords
// if (VERIFY) {
//     if (!BENCHMARK)
//         printf("verifying sorted order by bucketIndex of final stored file...\n");
//     // process_memo_records(FILENAME_FINAL, MEMORY_SIZE_bytes / sizeof(MemoRecord));
//     process_memo_records_table2(FILENAME_TABLE2, num_records_in_bucket * rounds);
// }

// if (DEBUG)
//     printf("SUCCESS!\n");
