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
    printf("  -f, --tmp file NAME                   Temporary file name\n");
    printf("  -g, --final file NAME                 Final file name\n");
    printf("  -j, --tmp table2 file NAME            Temporary table2 file name\n");
    printf("  -2, --table2 file NAME                Use Table2 approach (should specify -f (table1 file), if table1 was created previously, turn off HASHGEN)\n");
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

int main(int argc, char* argv[]) {
    // printf("size of MemoTable2Record: %zu\n", sizeof(MemoTable2Record));
    // printf("size of MemoRecord2: %zu\n", sizeof(MemoRecord2));
    // Default values
    const char* approach = "for"; // Default approach
    int num_threads = 0; // 0 means OpenMP chooses
    int num_threads_io = 0;
    unsigned long long total_nonces = 1ULL << K; // 2^K iterations
    unsigned long long MEMORY_SIZE_MB = 1;
    int TOTAL_FILES = 2;
    bool MERGE = false;

    char FILENAME[100]; // Default output file name
    char* FILENAME_FINAL = NULL; // Default output file name
    char* FILENAME_TABLE2 = NULL;
    char* FILENAME_TABLE2_tmp = NULL;
    char* SEARCH_STRING = NULL;

    // Define long options
    static struct option long_options[] = {
        { "approach", required_argument, 0, 'a' },
        { "threads", required_argument, 0, 't' },
        { "threads_io", required_argument, 0, 'i' },
        { "exponent", required_argument, 0, 'K' },
        { "memory", required_argument, 0, 'm' },
        { "file", required_argument, 0, 'f' },
        { "file_final", required_argument, 0, 'g' },
        { "file_table2_tmp", required_argument, 0, '2' },
        { "file_table2", required_argument, 0, 'j' },
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
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:f:g:2:j:b:w:c:v:s:S:x:y:d:h:n:M:", long_options, &option_index)) != -1) {
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
            if (MEMORY_SIZE_MB < 64) {
                fprintf(stderr, "Memory size must be at least 64 MB.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'f':
            // FILENAME = optarg;
            writeData = true;
            break;
        case 'g':
            FILENAME_FINAL = optarg;
            writeDataFinal = true;
            break;
        case '2':
            FILENAME_TABLE2_tmp = optarg;
            writeDataTable2Tmp = true;
            break;
        case 'j':
            FILENAME_TABLE2 = optarg;
            writeDataTable2 = true;
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
            if (strcmp(optarg, "true") == 0) {
                MERGE = true;
                HASHGEN = false;
            } else {
                MERGE = false;
            }
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    if ((SEARCH || SEARCH_BATCH) && !FILENAME_TABLE2) {
        fprintf(stderr, "Error: Final file name (-g) is required for search operations.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set the number of threads if specified
    if (num_threads > 0) {
        omp_set_num_threads(num_threads);
    }

    if (num_threads_io == 0) {
        num_threads_io = 1;
    }

    // Display selected configurations
    if (!BENCHMARK) {
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

    if (!BENCHMARK) {
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
                printf("Temporary File              : %s\n", FILENAME);
            }
            if (writeDataFinal) {
                printf("Output File Table 1         : %s\n", FILENAME_FINAL);
            }
            if (writeDataTable2) {
                printf("Output File Table 2         : %s\n", FILENAME_TABLE2);
            }
        }
    }

    if (HASHGEN) {
        // Allocate memory
        buckets = (Bucket*)calloc(total_buckets, sizeof(Bucket));
        buckets_table2 = (BucketTable2*)calloc(total_buckets, sizeof(BucketTable2));

        if (buckets == NULL || buckets_table2 == NULL) {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        for (unsigned long long i = 0; i < total_buckets; i++) {
            buckets[i].records = (MemoRecord*)calloc(num_records_in_bucket, sizeof(MemoRecord));
            buckets_table2[i].records = (MemoTable2Record*)calloc(num_records_in_bucket, sizeof(MemoTable2Record));
            if (buckets[i].records == NULL || buckets_table2[i].records == NULL) {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
                exit(EXIT_FAILURE);
            }
        }

        double total_time = 0;

        for (unsigned long long f = 1; f <= TOTAL_FILES; f++) {
            double file_time = 0;

            uint8_t fileId[FILEID_SIZE];
            memcpy(fileId, &f, FILEID_SIZE);

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

            // unsigned long long MAX_NUM_HASHES = 1ULL << (NONCE_SIZE * 8);
            // if we want to overgenerate hashes to fill all buckets
            // if (FULL_BUCKETS)
            // {
            //     num_records_per_round = MAX_NUM_HASHES / rounds;
            // }
            // unsigned long long nonce_max = 0;

            // printf("MAX_NUM_HASHES=%llu rounds=%llu num_records_per_round=%llu start_idx = %llu, end_idx = %llu\n", MAX_NUM_HASHES, rounds, num_records_per_round, start_idx, end_idx);

            // Recursive task based parallelism
            //             if (strcmp(approach, "xtask") == 0)
            //             {
            //                 srand((unsigned)time(NULL)); // Seed the random number generator
            // #pragma omp parallel
            //                 {
            // #pragma omp single
            //                     {
            //                         // could implement BATCH_SIZE here too
            //                         generateHashes(start_idx, end_idx);
            //                     }
            //                 }
            //             }
            //             else if (strcmp(approach, "task") == 0)
            //             {
            // // Tasking Model Approach
            // #pragma omp parallel
            //                 {
            // // #pragma omp single nowait
            // #pragma omp single
            //                     {
            //                         for (unsigned long long i = start_idx; i < end_idx; i += BATCH_SIZE)
            //                         {
            // #pragma omp task untied
            //                             {
            //                                 MemoRecord record;
            //                                 uint8_t hash[HASH_SIZE];

            //                                 unsigned long long batch_end = i + BATCH_SIZE;
            //                                 if (batch_end > end_idx)
            //                                 {
            //                                     batch_end = end_idx;
            //                                 }

            //                                 for (unsigned long long j = i; j < batch_end; j++)
            //                                 {
            //                                     generateBlake3(hash, &record, j);
            //                                     if (MEMORY_WRITE)
            //                                     {
            //                                         off_t bucketIndex = getBucketIndex(hash);
            //                                         insert_record(buckets, &record, bucketIndex);
            //                                     }
            //                                 }
            //                             }
            //                         }
            //                     }
            //                 } // Implicit barrier ensures all tasks are completed before exiting
            //             }
            if (strcmp(approach, "for") == 0) {
                // Parallel For Loop Approach
                volatile int cancel_flag = 0; // Shared flag
                full_buckets_global = 0;

                int count = 0;

#pragma omp parallel for schedule(static) shared(cancel_flag)
                for (unsigned long long n = 0; n < total_nonces; n += BATCH_SIZE) {
                    if (cancel_flag) {
                        continue;
                    }

                    unsigned long long batch_end = n + BATCH_SIZE;
                    if (batch_end >= total_nonces) {
                        batch_end = total_nonces - 1;
                    }

                    MemoRecord record;
                    uint8_t hash[HASH_SIZE];

                    for (unsigned long long j = n; j <= batch_end; j++) {

                        // FIXME: Do i need a new one every single time????
                        // Generate Blake3 hash
                        memcpy(record.nonce, &j, NONCE_SIZE);
                        g(record.nonce, fileId, hash);

                        // TODO: Try combining fileid and nonce into one input vs two separate inputs for hash
                        off_t bucketIndex = getBucketIndex(hash);
                        insert_record(buckets, &record, bucketIndex);
                    }

                    // TODO:
                    //  Set the flag if the termination condition is met.
                    //  if (i > end_idx/2 && full_buckets_global == total_buckets) {
                    //  if (full_buckets_global >= total_buckets)
                    //  {
                    //      cancel_flag = 1;
                    //      if (i > nonce_max)
                    //          nonce_max = i;
                    //  }
                }
            }
#ifndef __cplusplus
            // Your C-specific code here
            else if (strcmp(approach, "tbb") == 0) {
                printf("TBB is not supported with C, use C++ compiler instead to build vaultx, exiting...\n");
                exit(1);
            }
#endif

            // Hash generation complete
            double hashgen_end_time = omp_get_wtime();
            double hashgen_time = hashgen_end_time - hashgen_start_time;
            file_time += hashgen_time;

            printf("[File %llu] %-40s: %.2fs\n", f, "Hash Generation Complete", hashgen_time);

            // TODO: Parallelize storage efficiency calculations
            if (VERIFY) {
                unsigned long long num_zero = 0;

                for (unsigned long long i = 0; i < total_buckets; i++) {
                    num_zero += num_records_in_bucket - buckets[i].count;
                }

                printf("[File %llu] %-40s: %.2f%%\n", current_file, "Table 1 Storage efficiency", 100 * (1 - ((double)num_zero / total_nonces)));
            }

            // Sort hashes
            double sort_start_time = omp_get_wtime();

#pragma omp parallel for schedule(static)
            for (unsigned long long i = 0; i < total_buckets; i++) {
                // FIXME: Optimizatioon opportunity?
                // Hashes are recomputed a little
                qsort(buckets[i].records, buckets[i].count, sizeof(MemoRecord), compare_memo_all_record);
            }

            double sort_end_time = omp_get_wtime();
            double sort_time = sort_end_time - sort_start_time;
            file_time += sort_time;

            printf("[File %llu] %-40s: %.3fs\n", f, "Sorting Complete", sort_time);
            // Bucket* bucket = &buckets[100];
            // for (int j = 0; j < bucket->count; j++) {

            //     uint8_t hashA[HASH_SIZE];

            //     blake3_hasher hasher;
            //     blake3_hasher_init(&hasher);
            //     blake3_hasher_update(&hasher, &current_file, FILEID_SIZE);
            //     blake3_hasher_update(&hasher, bucket->records[j].nonce, NONCE_SIZE);
            //     blake3_hasher_finalize(&hasher, hashA, HASH_SIZE);

            //     printf("Hash: %s, Nonce: %llu\n", byteArrayToHexString(hashA, HASH_SIZE), byteArrayToLongLong(bucket->records[j].nonce, NONCE_SIZE));
            // }

            double matching_start_time = omp_get_wtime();

            // #pragma omp parallel for schedule(static)

            //             for (int i = 0; i < total_buckets; i++) {
            //                 generate_table2(buckets[i].records, num_records_in_bucket);
            //             }

            // FIXME: How to use a good bucket distance
            // Ideas: Average distance, multiply expected distance by a constant
            uint64_t expected_distance = 1ULL << (64 - K);

#pragma omp parallel for schedule(static)
            for (unsigned long long b = 0; b < total_buckets; b++) {
                Bucket* bucket = &buckets[b];

                for (unsigned long long i = 0; i + 1 < bucket->count; i++) {

                    // Calculate expected distance
                    // uint8_t* nonce_min = bucket->records[0].nonce;
                    // uint8_t* nonce_max = bucket->records[bucket->count - 1].nonce;
                    // uint8_t hash_max[HASH_SIZE];
                    // uint8_t hash_min[HASH_SIZE];
                    // g(nonce_min, fileId, hash_min);
                    // g(nonce_max, fileId, hash_max);
                    // uint64_t expected_distance = (double)compute_hash_distance(hash_max, hash_min, HASH_SIZE) / bucket->count;

                    uint8_t hash1[HASH_SIZE];
                    uint8_t hash2[HASH_SIZE];

                    g(bucket->records[i].nonce, fileId, hash1);

                    unsigned long long j = i + 1;

                    while (j < bucket->count) {
                        g(bucket->records[j].nonce, fileId, hash2);

                        uint64_t distance = compute_hash_distance(hash1, hash2, HASH_SIZE);
                        // printf("HashA: %s, Hashb: %s, Distance: %.2f\n", byteArrayToHexString(hash1, HASH_SIZE), byteArrayToHexString(hash2, HASH_SIZE), log2(distance));

                        if (distance > expected_distance) {
                            break;
                        }

                        MemoTable2Record record;
                        memcpy(record.nonce1, bucket->records[i].nonce, NONCE_SIZE);
                        memcpy(record.nonce2, bucket->records[j].nonce, NONCE_SIZE);

                        uint8_t hash[HASH_SIZE];
                        g2(record.nonce1, record.nonce2, fileId, hash);

                        size_t bucketIndex = getBucketIndex(hash);
                        insert_record2(buckets_table2, &record, bucketIndex);

                        j++;
                    }
                }
            }

            double matching_end_time = omp_get_wtime();
            double matching_time = matching_end_time - matching_start_time;
            file_time += matching_time;

            printf("[File %llu] %-40s: %.2fs\n", current_file, "Table 2 Generation Complete", matching_time);

            if (VERIFY) {
                size_t total_records = 0;

                for (unsigned long long i = 0; i < total_buckets; i++) {
                    total_records += buckets_table2[i].count;
                }

                printf("[File %llu] %-40s: %.2f%%\n", current_file, "Table 2 Storage Efficiency", 100 * ((double)total_records / total_nonces));
            }

            // for (int s = 0; s < total_buckets; s++) {
            //     BucketTable2* b = &buckets_table2[s];
            //     // printf("%d\n", b->count);
            //     if (b->count > 0) {
            //         printf("%d\n", b->count);
            //         for (int j = 0; j < b->count; j++) {

            //             uint8_t* nonce1 = b->records[j].nonce1;
            //             uint8_t* nonce2 = b->records[j].nonce2;

            //             uint8_t hash1[HASH_SIZE];
            //             uint8_t hash2[HASH_SIZE];

            //             g(nonce1, &current_file, hash1);
            //             g(nonce2, &current_file, hash2);

            //             uint8_t hash3[HASH_SIZE];

            //             g2(nonce1, nonce2, fileId, hash3);

            //             printf("HashA: %s, Hashb: %s, Hash3: %s\n", byteArrayToHexString(hash1, HASH_SIZE), byteArrayToHexString(hash2, HASH_SIZE), byteArrayToHexString(hash3, HASH_SIZE));
            //         }

            //         printf("\n\n");
            //     }
            // }

            // Write Table 2 to disk
            FILE* fd = NULL;
            snprintf(FILENAME, sizeof(FILENAME), "file%llu.plot", f);

            double fileio_start_time = omp_get_wtime();

            if (writeData) {
                fd = fopen(FILENAME, "wb+");

                if (fd == NULL) {
                    printf("Error opening file %s (#4)\n", FILENAME);
                    perror("Error opening file");
                    return EXIT_FAILURE;
                }

                for (unsigned long long i = 0; i < total_buckets; i++) {
                    size_t elements_written = fwrite(buckets_table2[i].records, sizeof(MemoTable2Record), num_records_in_bucket, fd);
                    if (elements_written != num_records_in_bucket) {
                        fprintf(stderr, "Error writing bucket to file");
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }
                }
                // Flush memory buffer to disk and close the file

                if (fflush(fd) != 0) {
                    perror("Failed to flush buffer");
                    fclose(fd);
                    return EXIT_FAILURE;
                }

                fclose(fd);
            }

            double fileio_end_time = omp_get_wtime();
            double fileio_time = fileio_end_time - fileio_start_time;
            file_time += fileio_time;

            // Clearing memory of all buckets
            if (current_file < TOTAL_FILES) {
                double empty_start_time = omp_get_wtime();

                for (unsigned long long i = 0; i < total_buckets; i++) {
                    memset(buckets[i].records, 0, sizeof(MemoRecord) * num_records_in_bucket);
                    memset(buckets_table2[i].records, 0, sizeof(MemoTable2Record) * num_records_in_bucket);
                }

                double empty_end_time = omp_get_wtime();
                double empty_time = empty_end_time - empty_start_time;
                file_time += empty_time;

                printf("[File %llu] %-40s: %.2fs\n", current_file, "Clearing bucket memory for next file", empty_time);
            }

            total_time += file_time;

            printf("[File %llu] %-40s: %.2fs\n", current_file, "Finished writing to disk", fileio_time);
            printf("File %llu   %-40s: %.2fs\n\n\n", current_file, "Processing Complete", file_time);

            // Prepare for next file
            current_file++;
            full_buckets_global = 0;

            // Calculate throughput (hashes per second)
            // TODO: Throughput calculation
            // throughput_hash = (num_records_per_round / (hashgen_time + elapsed_time_io)) / (1e6);

            // // Calculate I/O throughput
            // if (rounds > 1)
            // {
            //     throughput_io = (num_records_per_round * sizeof(MemoRecord)) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);
            // }
            // else
            // {
            //     throughput_io = (num_records_per_round * sizeof(MemoTable2Record)) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);
            // }

            // Check Bucket Efficiency for this round

            //  if (!BENCHMARK)
            // printf("[%.2f] HashGen %.2f%%: %.2f MH/s : I/O %.2f MB/s\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds, throughput_hash, throughput_io);
            // end of loop

            //  start_time_io = omp_get_wtime();
        }

        printf("[%.2fs] Completed generating %d files\n", total_time, TOTAL_FILES);
    }

    if (MERGE) {
        remove("merge.plot");

        double start_time = omp_get_wtime();
        printf("\n\nMemory Size: %lluMB\n", MEMORY_SIZE_MB);
        printf("File Size: %.0fMB\n", file_size_gb * 1024);

        unsigned long long records_per_global_bucket = TOTAL_FILES * num_records_in_bucket;
        unsigned long long global_bucket_size = records_per_global_bucket * sizeof(MemoTable2Record);

        // Number of global buckets that can be stored in memory
        unsigned long long total_global_buckets = (unsigned long long)floor(MEMORY_SIZE_MB * 1024 * 1024 / global_bucket_size);

        printf("Total Global Buckets Capacity in memory: %llu\n", total_global_buckets);

        // Allocate memory
        MemoTable2Record* mergedBuckets = (MemoTable2Record*)calloc(total_global_buckets * global_bucket_size, sizeof(MemoTable2Record));

        if (mergedBuckets == NULL) {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        FILE* merge_fd = NULL;
        // snprintf(FILENAME, sizeof(FILENAME), "file%llu.plot", f);

        // double fileio_start_time = omp_get_wtime();

        // Delete file if present initially

        merge_fd = fopen("merge.plot", "ab");

        if (merge_fd == NULL) {
            perror("Error opening file");
            return EXIT_FAILURE;
        }

        for (int i = 0; i < total_buckets; i += total_global_buckets) {
            unsigned long long end = i + total_global_buckets;
            if (end > total_buckets) {
                end = total_buckets;
            }

            for (int f = 1; f <= TOTAL_FILES; f++) {
                char filename[100];
                snprintf(filename, sizeof(filename), "file%d.plot", f);

                FILE* fd = fopen(filename, "rb");
                if (!fd) {
                    perror("Error opening file");
                    return 1;
                }

                size_t merged_pos = (f - 1) * num_records_in_bucket;

                size_t offset = i * num_records_in_bucket * sizeof(MemoTable2Record);

                if (fseek(fd, offset, SEEK_SET) != 0) {
                    perror("fseek failed");
                    fclose(fd);
                    return 1;
                }

                for (unsigned long long bucketIndex = i; bucketIndex < end; bucketIndex++) {
                    size_t read_bytes = fread(&mergedBuckets[merged_pos], sizeof(MemoTable2Record), num_records_in_bucket, fd);
                    if (read_bytes != num_records_in_bucket) {
                        if (feof(fd)) {
                            printf("Reached end of file after reading %zu bytes\n", read_bytes);
                        } else {
                            perror("fread failed");
                            fclose(fd);
                            return 1;
                        }
                    }

                    merged_pos += records_per_global_bucket;
                }

                fclose(fd);
            }

            // Write data to file

            size_t elements_written = fwrite(mergedBuckets, sizeof(MemoTable2Record), (end - i) * records_per_global_bucket, merge_fd);
            if (elements_written != (end - i) * records_per_global_bucket) {
                fprintf(stderr, "Error writing bucket to file");
                fclose(merge_fd);
                exit(EXIT_FAILURE);
            }

            // double fileio_end_time = omp_get_wtime();
            // double fileio_time = fileio_end_time - fileio_start_time;
            // file_time += fileio_time;

            // Clear memory
            if (end != total_buckets) {
                memset(mergedBuckets, 0, total_global_buckets * global_bucket_size);
            }
        }

        printf("Merge complete: %.2fs\n\n\n", omp_get_wtime() - start_time);

        // Flush memory buffer to disk and close the file

        if (fflush(merge_fd) != 0) {
            perror("Failed to flush buffer");
            fclose(merge_fd);
            return EXIT_FAILURE;
        }

        fclose(merge_fd);

        // if (VERIFY) {
        //     int bucketIndex = 100;
        //     char* filename = "merge.plot";

        //     printf("Bucket Index: %d (Prefix: %s)\n", bucketIndex, num_to_hex(bucketIndex, PREFIX_SIZE));

        //     FILE* fd = fopen(filename, "rb");
        //     if (!fd) {
        //         perror("Error opening file");
        //         return 1;
        //     }

        //     size_t offset = bucketIndex * global_bucket_size;

        //     if (fseek(fd, offset, SEEK_SET) != 0) {
        //         perror("fseek failed");
        //         fclose(fd);
        //         return 1;
        //     }

        //     MemoTable2Record* global_bucket = (MemoTable2Record*)calloc(records_per_global_bucket, sizeof(MemoTable2Record));

        //     if (global_bucket == NULL) {
        //         perror("calloc failed");
        //         fclose(fd);
        //         return 1;
        //     }

        //     size_t elements_written = fread(global_bucket, sizeof(MemoTable2Record), records_per_global_bucket, fd);
        //     if (elements_written != records_per_global_bucket) {
        //         fprintf(stderr, "Error writing bucket to file");
        //         fclose(fd);
        //         exit(EXIT_FAILURE);
        //     }

        //     for (int i = 0; i < records_per_global_bucket; i++) {
        //         uint8_t* nonce1;
        //         uint8_t* nonce2;
        //         int fID = (i / num_records_in_bucket) + 1;
        //         uint8_t* fileID[FILEID_SIZE];
        //         mempcpy(fileID, &fID, FILEID_SIZE);

        //         //     memcpy(nonce1, global_bucket[i].nonce1, NONCE_SIZE);
        //         //    memcpy(nonce2, global_bucket[i].nonce2, NONCE_SIZE);

        //         uint8_t hash[HASH_SIZE];

        //         g2(global_bucket[i].nonce1, global_bucket[i].nonce2, fileID, hash);

        //         if (byteArrayToLongLong(global_bucket[i].nonce1, NONCE_SIZE) == 0 && byteArrayToLongLong(global_bucket[i].nonce2, NONCE_SIZE) == 0) {
        //             printf("Zero Nonce\n");
        //         } else {
        //             printf("Hash: %s\n", byteArrayToHexString(hash, HASH_SIZE));
        //         }
        //     }
        // }

        printf("Starting verification: \n");
        if (VERIFY) {
            char* filename = "merge.plot";
            size_t verified_global_buckets = 0;

#pragma omp parallel for
            for (unsigned long long i = 0; i < total_buckets; i += BATCH_SIZE) {

                FILE* fd = fopen(filename, "rb");
                if (!fd) {
                    perror("Error opening file");
                    // return 1;
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
                uint8_t fileId[FILEID_SIZE];
                uint8_t hash[HASH_SIZE];

                for (unsigned long long j = i; j < end; j++) {
                    bool flag = true;

                    for (int k = 0; k < records_per_global_bucket; k++) {
                        record = &buffer[(j - i) * records_per_global_bucket + k];
                        int fId = (k / num_records_in_bucket) + 1;
                        memcpy(fileId, &fId, FILEID_SIZE);

                        if (byteArrayToLongLong(record->nonce1, NONCE_SIZE) == 0 && byteArrayToLongLong(record->nonce2, NONCE_SIZE) == 0) {
                        } else {
                            g2(record->nonce1, record->nonce2, fileId, hash);
                            if (byteArrayToLongLong(hash, PREFIX_SIZE) != j) {
                                flag = false;
                            }
                        }

                        if (!flag) {
                            break;
                        }
                    }

                    if (flag) {
#pragma omp atomic
                        verified_global_buckets++;
                    } else {
                        printf("J: %llu\n", j);
                    }
                }

                fclose(fd);
            }

            printf("Total Buckets: %llu\nVerified GBs: %llu\n", total_buckets, verified_global_buckets);
        }
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
