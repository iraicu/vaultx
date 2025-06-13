#include "vaultx.h"

// Function to display usage information
void print_usage(char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -a, --approach [xtask|task|for|tbb]   Select parallelization approach (default: for)\n");
    printf("  -t, --threads NUM                     Number of threads to use (default: number of available cores)\n");
    printf("  -i, --threads_io NUM                  Number of I/O threads (default: number of available cores)\n");
    printf("  -K, --exponent NUM                    Exponent K to compute 2^K number of records (default: 4)\n");
    printf("  -m, --memory NUM                      Memory size in MB (default: 1)\n");
    printf("  -b, --batch-size NUM                  Batch size (default: 1024)\n");
    printf("  -f, --file tmp NAME                   Temporary table1 (out-of-memory)/table2 (in-memory) file name\n");
    printf("  -g, --file tmp table2 NAME            Temporary table2 file name (only for out-of-memory)\n");
    printf("  -j, --file table2 NAME                Table2 file name\n");
    // printf("  -2, --table2 file NAME                Use Table2 approach (should specify -f (table1 file), if table1 was created previously, turn off HASHGEN)\n");
    printf("  -s, --search STRING                   Search for a specific hash prefix in the file\n");
    printf("  -S, --search-batch NUM                Search for a specific hash prefix in the file in batch mode\n");
    printf("  -x, --benchmark                       Enable benchmark mode (default: false)\n");
    printf("  -h, --help                            Display this help message\n");
    printf("\nExample:\n");
    printf("IN-MEMORY:          %s -a for -t 8 -i 8 -K 28 -m 1024 -f memo.t -j memo.x\n", prog_name);
    printf("IN-MEMORY BENCH:    %s -a for -t 8 -i 8 -K 28 -m 1024 -f memo.t -j memo.x -x true (Only prints time)\n", prog_name);
    printf("OUT-OF-MEMORY:      %s -a for -t 8 -K 28 -m 512 -f memo.t -g memo2.t -j memo.x\n", prog_name);
    // printf("SEARCH:             %s -a for -t 8 -K 28 -m 1024 -f memo.t -j memo.x -s 000000\n", prog_name);
}

void compute_hashed_key_from_plot_id()
{
    uint8_t key[33];
    memcpy(key, plot_id, 32);
    key[32] = K;

    crypto_hash_sha256(hashed_key, key, sizeof(key));
}

int generate_plot_id()
{
    uint8_t public_key[32];
    // uint8_t pubkey_compressed[33];
    // size_t public_key_len = sizeof(pubkey_compressed);

    // generate 32 random bytes for the private key
    randombytes_buf(public_key, 32);

    // create a secp256k1 context for signing
    // secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    // secp256k1_pubkey pubkey;

    // if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key))
    // {
    //     fprintf(stderr, "Error creating public key\n");
    //     secp256k1_context_destroy(ctx);
    //     return -1;
    // }

    // serialize the public key
    // secp256k1_ec_pubkey_serialize(ctx, pubkey_compressed, &public_key_len, &pubkey, SECP256K1_EC_COMPRESSED);

    // concat public and private keys
    // uint8_t combined[65];
    // memcpy(combined, pubkey_compressed, 33);
    // memcpy(combined + 33, private_key, 32);

    // compute SHA-256 of the combined buffer -> plot_id
    crypto_hash_sha256(plot_id, public_key, sizeof(public_key));

    // hash plot_id together with k-value to produce the key (unique plots)
    compute_hashed_key_from_plot_id();

    // secp256k1_context_destroy(ctx);

    return 0;
}

// Function to compute the bucket index based on hash prefix
off_t getBucketIndex(const uint8_t *hash)
{
    off_t index = 0;
    for (size_t i = 0; i < PREFIX_SIZE && i < HASH_SIZE; i++)
    {
        index = (index << 8) | hash[i];
    }
    return index;
}

// Function to convert bytes to unsigned long long
unsigned long long byteArrayToLongLong(const uint8_t *byteArray, size_t length)
{
    unsigned long long result = 0;
    for (size_t i = 0; i < length; ++i)
    {
        result = (result << 8) | (unsigned long long)byteArray[i];
    }
    return result;
}

// Function to check if a nonce is non-zero
bool is_nonce_nonzero(const uint8_t *nonce, size_t nonce_size)
{
    // Check for NULL pointer
    if (nonce == NULL)
    {
        // Handle error as needed
        return false;
    }

    // Iterate over each byte of the nonce
    for (size_t i = 0; i < nonce_size; ++i)
    {
        if (nonce[i] != 0)
        {
            // Found a non-zero byte
            return true;
        }
    }

    // All bytes are zero
    return false;
}

int hex_string_to_byte_array(const char *hex_string, uint8_t *out, size_t out_len)
{
    size_t hexLen = strlen(hex_string);
    if (hexLen % 2 != 0 || out_len < hexLen / 2)
    {
        return -1; // Invalid length or not enough space
    }

    for (size_t i = 0; i < hexLen / 2; ++i)
    {
        if (sscanf(&hex_string[i * 2], "%2hhx", &out[i]) != 1)
        {
            return -2; // Parsing error
        }
    }

    return 0; // success
}

void bytes_to_hex(const uint8_t *bytes, size_t len, char *out_hex)
{
    for (size_t i = 0; i < len; i++)
    {
        sprintf(out_hex + i * 2, "%02x", bytes[i]);
    }
    out_hex[len * 2] = '\0'; // Null-terminate the string
}

uint64_t largest_power_of_two_less_than(uint64_t number)
{
    if (number == 0)
    {
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

int main(int argc, char *argv[])
{
    // printf("size of MemoTable2Record: %zu\n", sizeof(MemoTable2Record));
    // printf("size of MemoRecord2: %zu\n", sizeof(MemoRecord2));
    // Default values
    const char *approach = "for"; // Default approach
    int num_threads = 0;          // 0 means OpenMP chooses
    int num_threads_io = 0;
    unsigned long long num_records_total = 1ULL << K; // 2^K iterations
    unsigned long long num_records_per_round = num_records_total;
    unsigned long long MEMORY_SIZE_MB = 1;
    char *DIR_TMP = NULL; // Default output file name
    char *DIR_TMP_TABLE2 = NULL;
    char *DIR_TABLE2 = NULL;
    char *SEARCH_STRING = NULL;

    char FILENAME_TMP[150];
    char FILENAME_TMP_TABLE2[150];
    char FILENAME_TABLE2[150];

    // Define long options
    static struct option long_options[] = {
        {"approach", required_argument, 0, 'a'},
        {"threads", required_argument, 0, 't'},
        {"threads_io", required_argument, 0, 'i'},
        {"exponent", required_argument, 0, 'K'},
        {"memory", required_argument, 0, 'm'},
        {"file_tmp", required_argument, 0, 'f'},
        {"file_tmp_table2", required_argument, 0, 'g'},
        {"file_table2", required_argument, 0, 'j'},
        {"batch_size", required_argument, 0, 'b'},
        {"memory_write", required_argument, 0, 'w'},
        {"circular_array", required_argument, 0, 'c'},
        {"verify", required_argument, 0, 'v'},
        {"search", required_argument, 0, 's'},
        {"prefix_search_size", required_argument, 0, 'S'},
        {"benchmark", required_argument, 0, 'x'},
        {"full_buckets", required_argument, 0, 'y'},
        {"debug", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:f:g:j:b:w:c:v:s:S:x:y:d:h", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'a':
            if (strcmp(optarg, "xtask") == 0 || strcmp(optarg, "task") == 0 || strcmp(optarg, "for") == 0 || strcmp(optarg, "tbb") == 0)
            {
                approach = optarg;
            }
            else
            {
                fprintf(stderr, "Invalid approach: %s\n", optarg);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            num_threads = atoi(optarg);
            if (num_threads <= 0)
            {
                fprintf(stderr, "Number of threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            num_threads_io = atoi(optarg);
            if (num_threads_io <= 0)
            {
                fprintf(stderr, "Number of I/O threads must be positive.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'K':
            K = atoi(optarg);
            if (K < 24 || K > 40)
            { // Limiting K to avoid overflow
                fprintf(stderr, "Exponent K must be between 24 and 40.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            num_records_total = 1ULL << K; // Compute 2^K
            break;
        case 'm':
            MEMORY_SIZE_MB = atoi(optarg);
            if (MEMORY_SIZE_MB < 64)
            {
                fprintf(stderr, "Memory size must be at least 64 MB.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'f':
            DIR_TMP = optarg;
            writeDataTmp = true;
            break;
        case 'g':
            DIR_TMP_TABLE2 = optarg;
            writeDataTmpTable2 = true;
            break;
        case 'j':
            DIR_TABLE2 = optarg;
            writeDataTable2 = true;
            break;
        case 'b':
            BATCH_SIZE = atoi(optarg);
            if (BATCH_SIZE < 1)
            {
                fprintf(stderr, "BATCH_SIZE must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'w':
            if (strcmp(optarg, "true") == 0)
            {
                MEMORY_WRITE = true;
            }
            else
            {
                MEMORY_WRITE = false;
            }
            break;
        case 'c':
            if (strcmp(optarg, "true") == 0)
            {
                CIRCULAR_ARRAY = true;
            }
            else
            {
                CIRCULAR_ARRAY = false;
            }
            break;
        case 'v':
            if (strcmp(optarg, "true") == 0)
            {
                VERIFY = true;
            }
            else
            {
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
            if (PREFIX_SEARCH_SIZE < 1)
            {
                fprintf(stderr, "PREFIX_SEARCH_SIZE must be 1 or greater.\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'x':
            if (strcmp(optarg, "true") == 0)
            {
                BENCHMARK = true;
            }
            else
            {
                BENCHMARK = false;
            }
            break;
        case 'y':
            if (strcmp(optarg, "true") == 0)
            {
                FULL_BUCKETS = true;
            }
            else
            {
                FULL_BUCKETS = false;
            }
            break;
        case 'd':
            if (strcmp(optarg, "true") == 0)
            {
                DEBUG = true;
            }
            else
            {
                DEBUG = false;
            }
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    if ((SEARCH || SEARCH_BATCH) && !writeDataTable2)
    {
        fprintf(stderr, "Error: Final file name (-j) is required for search operations.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set the number of threads if specified
    if (num_threads > 0)
    {
        omp_set_num_threads(num_threads);
    }

    if (num_threads_io == 0)
    {
        num_threads_io = 1;
    }

    // Display selected configurations
    if (!BENCHMARK)
    {
        if (!SEARCH)
        {
            printf("Selected Approach           : %s\n", approach);
            printf("Number of Threads           : %d\n", num_threads > 0 ? num_threads : omp_get_max_threads());
            printf("Number of Threads I/O       : %d\n", num_threads_io > 0 ? num_threads_io : omp_get_max_threads());
            printf("Exponent K                  : %d\n", K);
        }
    }

    unsigned long long file_size_bytes = num_records_total * NONCE_SIZE;
    double file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);
    unsigned long long MEMORY_SIZE_bytes = 0;

    if (MEMORY_SIZE_MB * 1024 * 1024 > file_size_bytes)
    {
        MEMORY_SIZE_MB = (unsigned long long)(file_size_bytes / (1024 * 1024));
        MEMORY_SIZE_bytes = file_size_bytes;
    }
    else
        MEMORY_SIZE_bytes = MEMORY_SIZE_MB * 1024 * 1024;

    // printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
    // printf("Memory Size (bytes)            : %llu\n", MEMORY_SIZE_bytes);

    rounds = ceil(file_size_bytes / MEMORY_SIZE_bytes);
    MEMORY_SIZE_bytes = file_size_bytes / rounds;
    num_records_per_round = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    MEMORY_SIZE_bytes = num_records_per_round * NONCE_SIZE;
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);

    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));

    num_records_per_round = MEMORY_SIZE_bytes / NONCE_SIZE;

    total_num_buckets = 1ULL << (PREFIX_SIZE * 8);

    num_records_in_bucket = num_records_per_round / total_num_buckets;
    num_records_in_shuffled_bucket = num_records_in_bucket * rounds;

    MEMORY_SIZE_bytes = total_num_buckets * num_records_in_bucket * sizeof(MemoRecord);
    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);
    num_records_per_round = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    num_records_total = num_records_per_round * rounds;

    if (!SEARCH || !SEARCH_BATCH)
    {

        if (sodium_init() < 0)
        {
            printf("libsodium failed to initialize.\n");
            return 1;
        }

        if (generate_plot_id() == 0)
        {
            char hex_plot_id[65];
            bytes_to_hex(plot_id, 32, hex_plot_id);

            sprintf(FILENAME_TMP, "%s%s.tmp", DIR_TMP, hex_plot_id);
            sprintf(FILENAME_TMP_TABLE2, "%s%s.tmp2", DIR_TMP_TABLE2, hex_plot_id);
            sprintf(FILENAME_TABLE2, "%s%s.plot", DIR_TABLE2, hex_plot_id);
        }
    }

    if (!BENCHMARK)
    {
        if (SEARCH)
        {
            printf("SEARCH                      : true\n");
        }
        else
        {
            printf("Table1 File Size (GB)       : %.2f\n", file_size_gb);
            printf("Table1 File Size (bytes)    : %llu\n", file_size_bytes);

            printf("Table2 File Size (GB)       : %.2f\n", file_size_gb * 2);
            printf("Table2 File Size (bytes)    : %llu\n", file_size_bytes * 2);

            printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
            printf("Memory Size (bytes)         : %llu\n", MEMORY_SIZE_bytes);

            printf("Number of Hashes (RAM)      : %llu\n", num_records_per_round);

            printf("Number of Hashes (Disk)     : %llu\n", num_records_total);
            printf("Size of MemoRecord          : %lu\n", sizeof(MemoRecord));
            printf("Rounds                      : %llu\n", rounds);

            printf("Number of Buckets           : %llu\n", total_num_buckets);
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

            if (writeDataTmp)
            {
                printf("File Table 1 (tmp)          : %s\n", FILENAME_TMP);
            }
            if (writeDataTmpTable2)
            {
                printf("File Table 2 (tmp)          : %s\n", FILENAME_TMP_TABLE2);
            }
            if (writeDataTable2)
            {
                printf("File Table 2                : %s\n", FILENAME_TABLE2);
            }
        }
    }

    if (HASHGEN)
    {
        // uint8_t hash1[12] = {0x00, 0x00, 0x05, 0x01, 0x85, 0xe7, 0x53, 0x2a, 0xea, 0x2f, 0xa6, 0x9c};
        // uint8_t hash2[12] = {0x00, 0x00, 0x05, 0x94,
        //                      0xd1, 0x95, 0x7e, 0xc2,
        //                      0xd9, 0xe2, 0x60, 0xf5};
        // uint64_t expected_distance = 1ULL << (64 - K);
        // uint64_t distance = compute_hash_distance(hash1, hash2, 12);
        // printf("%ld\n", expected_distance);
        // printf("%ld\n", distance);

        // return -1;

        if (!writeDataTmp)
        {
            fprintf(stderr, "Error: Table1 file name (-f) is required for hash generation.\n");
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        if (!BENCHMARK)
            printf("------------------------Table 1 generation started------------------------\n");

        // Open tmp file for writing 1) if in-memory: table2, 2) if out-of-memory: unshuffled table1
        FILE *fd_tmp = NULL;
        fd_tmp = fopen(FILENAME_TMP, "wb+");
        if (fd_tmp == NULL)
        {
            printf("Error opening file %s (#4)\n", FILENAME_TMP);

            perror("Error opening file");
            return EXIT_FAILURE;
        }

        // Start walltime measurement
        double start_time = omp_get_wtime();

        // Allocate memory for the array of Buckets
        buckets = (Bucket *)calloc(total_num_buckets, sizeof(Bucket));
        buckets2 = (BucketTable2 *)calloc(total_num_buckets, sizeof(BucketTable2));

        if (buckets == NULL || buckets2 == NULL)
        {
            fprintf(stderr, "Error: Unable to allocate memory for buckets2.\n");
            exit(EXIT_FAILURE);
        }

        for (unsigned long long i = 0; i < total_num_buckets; i++)
        {
            buckets[i].records = (MemoRecord *)calloc(num_records_in_bucket, sizeof(MemoRecord));
            if (buckets[i].records == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
                exit(EXIT_FAILURE);
            }

            buckets2[i].records = (MemoTable2Record *)calloc(num_records_in_bucket, sizeof(MemoTable2Record));
            if (buckets2[i].records == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for records in table 2.\n");
                exit(EXIT_FAILURE);
            }
        }

        double throughput_hash = 0.0;
        double throughput_io = 0.0;

        double start_time_io = 0.0;
        double end_time_io = 0.0;
        double elapsed_time_io = 0.0;
        double elapsed_time_io2 = 0.0;
        double start_time_hash = 0.0;
        double end_time_hash = 0.0;
        double elapsed_time_hash = 0.0;

        double elapsed_time_hash_total = 0.0;
        double elapsed_time_io_total = 0.0;
        double elapsed_time_io2_total = 0.0;

        // Generate table 1; 1) if in-memory: also generate table2 and write to disk,
        // 2) if out-of-memory: only generate unshuffled table1 and write to disk
        for (unsigned long long r = 0; r < rounds; r++)
        {
            start_time_hash = omp_get_wtime();

            // Reset bucket counts
            for (unsigned long long i = 0; i < total_num_buckets; i++)
            {
                buckets[i].count = 0;
                buckets[i].count_waste = 0;
                buckets[i].full = false;
                buckets[i].flush = 0;

                buckets2[i].count = 0;
                buckets2[i].count_waste = 0;
                buckets2[i].full = false;
                buckets2[i].flush = 0;
            }

            unsigned long long MAX_NUM_HASHES = 1ULL << (NONCE_SIZE * 8);
            // if we want to overgenerate hashes to fill all buckets
            if (FULL_BUCKETS)
                num_records_per_round = MAX_NUM_HASHES / rounds;
            unsigned long long start_idx = r * num_records_per_round;
            unsigned long long end_idx = start_idx + num_records_per_round;
            unsigned long long nonce_max = 0;

            if (DEBUG)
                printf("MAX_NUM_HASHES=%llu rounds=%llu num_records_per_round=%llu start_idx=%llu, end_idx=%llu\n", MAX_NUM_HASHES, rounds, num_records_per_round, start_idx, end_idx);

            // Recursive task based parallelism
            if (strcmp(approach, "xtask") == 0)
            {
                srand((unsigned)time(NULL)); // Seed the random number generator
#pragma omp parallel
                {
#pragma omp single
                    {
                        // could implement BATCH_SIZE here too
                        generateHashes(start_idx, end_idx);
                    }
                }
            }
            else if (strcmp(approach, "task") == 0)
            {
// Tasking Model Approach
#pragma omp parallel
                {
// #pragma omp single nowait
#pragma omp single
                    {
                        for (unsigned long long i = start_idx; i < end_idx; i += BATCH_SIZE)
                        {
#pragma omp task untied
                            {
                                MemoRecord record;
                                uint8_t record_hash[HASH_SIZE];

                                unsigned long long batch_end = i + BATCH_SIZE;
                                if (batch_end > end_idx)
                                {
                                    batch_end = end_idx;
                                }

                                for (unsigned long long j = i; j < batch_end; j++)
                                {
                                    generateBlake3(record_hash, &record, j);
                                    if (MEMORY_WRITE)
                                    {
                                        off_t bucketIndex = getBucketIndex(record_hash);
                                        insert_record(buckets, &record, bucketIndex);
                                    }
                                }
                            }
                        }
                    }
                } // Implicit barrier ensures all tasks are completed before exiting
            }
            else if (strcmp(approach, "for") == 0)
            {
                // Parallel For Loop Approach
                volatile int cancel_flag = 0; // Shared flag
                full_buckets_global = 0;
#pragma omp parallel for schedule(static) shared(cancel_flag)
                for (unsigned long long i = start_idx; i < end_idx; i += BATCH_SIZE)
                {
                    if (cancel_flag)
                        continue;

                    MemoRecord record;
                    uint8_t record_hash[HASH_SIZE];

                    unsigned long long batch_end = i + BATCH_SIZE;
                    if (batch_end > end_idx)
                    {
                        batch_end = end_idx;
                    }

                    for (unsigned long long j = i; j < batch_end; j++)
                    {
                        generateBlake3(record_hash, &record, j);
                        if (MEMORY_WRITE)
                        {
                            off_t bucketIndex = getBucketIndex(record_hash);
                            insert_record(buckets, &record, bucketIndex);
                        }
                    }

                    // Set the flag if the termination condition is met.
                    // if (i > end_idx/2 && full_buckets_global == total_num_buckets) {
                    if (full_buckets_global >= total_num_buckets)
                    {
                        cancel_flag = 1;
                        if (i > nonce_max)
                            nonce_max = i;
                    }
                }
            }
#ifndef __cplusplus
            // Your C-specific code here
            else if (strcmp(approach, "tbb") == 0)
            {
                printf("TBB is not supported with C, use C++ compiler instead to build vaultx, exiting...\n");
                exit(1);
            }
#endif

#ifdef __cplusplus
            // Your C++-specific code here
            else if (strcmp(approach, "tbb") == 0)
            {
                // Parallel For Loop Approach
                tbb::parallel_for(
                    tbb::blocked_range<unsigned long long>(start_idx, end_idx, BATCH_SIZE),
                    [&](const tbb::blocked_range<unsigned long long> &batch_range)
                    {
                        // Process each batch in the range
                        for (unsigned long long i = batch_range.begin(); i < batch_range.end(); i += BATCH_SIZE)
                        {
                            unsigned long long batch_end = i + BATCH_SIZE;
                            if (batch_end > end_idx)
                            {
                                batch_end = end_idx;
                            }

                            for (unsigned long long j = i; j < batch_end; ++j)
                            {
                                MemoRecord record;
                                uint8_t record_hash[HASH_SIZE];

                                generateBlake3(record_hash, &record, j);

                                if (MEMORY_WRITE)
                                {
                                    off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                                    insert_record(buckets, &record, bucketIndex);
                                }
                            }
                        }
                    });
            }
#endif

            // after else if
            // End hash computation time measurement
            end_time_hash = omp_get_wtime();
            elapsed_time_hash = end_time_hash - start_time_hash;
            elapsed_time_hash_total += elapsed_time_hash;

            // If tmp file is specified and data fits in memory, generate table2 and write to tmp file
            if (rounds == 1)
            {
                printf("--------------------In-memory Table 2 generation started------------------\n");
                start_time_io = omp_get_wtime();

                // Seek to the correct position in the file
                off_t offset = r * total_num_buckets * num_records_in_bucket * sizeof(MemoTable2Record); // total_num_buckets that fit in memory
                if (fseeko(fd_tmp, offset, SEEK_SET) < 0)
                {
                    perror("Error seeking in file");
                    fclose(fd_tmp);
                    exit(EXIT_FAILURE);
                }

                size_t bytesWritten = 0;
                // Set the number of threads if specified
                if (num_threads_io > 0)
                {
                    omp_set_num_threads(num_threads);
                }

// Generate Table2
#pragma omp parallel for schedule(static)
                for (unsigned long long i = 0; i < total_num_buckets; i++)
                {
                    sort_bucket_records_inplace(buckets[i].records, num_records_in_bucket);
                    generate_table2(buckets[i].records, num_records_in_bucket);
                }

                // Write table2 to disk
                for (unsigned long long i = 0; i < total_num_buckets; i++)
                {
                    size_t elements_written = fwrite(buckets2[i].records, sizeof(MemoTable2Record), num_records_in_bucket, fd_tmp);
                    if (elements_written != num_records_in_bucket)
                    {
                        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                                elements_written, num_records_in_bucket);
                        fclose(fd_tmp);
                        exit(EXIT_FAILURE);
                    }
                    bytesWritten += elements_written * sizeof(MemoTable2Record);
                }

                // End I/O time measurement
                end_time_io = omp_get_wtime();
                elapsed_time_io = end_time_io - start_time_io;
                elapsed_time_io_total += elapsed_time_io;

                // count how many full buckets , this works for rounds == 1
                unsigned long long full_buckets = 0;
                unsigned long long record_counts = 0;
                unsigned long long record_counts_waste = 0;
                for (unsigned long long i = 0; i < total_num_buckets; i++)
                {
                    if (buckets2[i].count == num_records_in_bucket)
                        full_buckets++;
                    record_counts += buckets2[i].count;
                    record_counts_waste += buckets2[i].count_waste;
                }

                if (!BENCHMARK)
                    printf("record_counts=%llu storage_efficiency_table2=%.2f full_buckets=%llu bucket_efficiency=%.2f nonce_max=%llu record_counts_waste=%llu hash_efficiency=%.2f\n", record_counts, record_counts * 100.0 / (total_num_buckets * num_records_in_bucket), full_buckets, full_buckets * 100.0 / total_num_buckets, nonce_max, record_counts_waste, total_num_buckets * num_records_in_bucket * 100.0 / (record_counts_waste + total_num_buckets * num_records_in_bucket));

                off_t bucket_idx = 5;
                // print contents of a bucket
                for (unsigned long long i = 0; i < num_records_in_bucket; i++)
                {
                    printf("Nonce1 : ");

                    for (unsigned long long j = 0; j < NONCE_SIZE; j++)
                    {
                        printf("%02x", buckets2[bucket_idx].records[i].nonce1[j]);
                    }
                    printf(" | Nonce2 : ");
                    for (unsigned long long j = 0; j < NONCE_SIZE; j++)
                    {
                        printf("%02x", buckets2[bucket_idx].records[i].nonce2[j]);
                    }
                    printf(" | Hash : ");
                    uint8_t hash[HASH_SIZE];
                    MemoTable2Record *record = malloc(sizeof(MemoTable2Record));
                    if (record == NULL)
                    {
                        fprintf(stderr, "Error: Unable to allocate memory for record.\n");
                        exit(EXIT_FAILURE);
                    }
                    generate_hash2(buckets2[bucket_idx].records[i].nonce1, buckets2[bucket_idx].records[i].nonce2, hash);

                    for (unsigned long long j = 0; j < HASH_SIZE; j++)
                    {
                        printf("%02x", hash[j]);
                    }
                    printf("\n");
                }
            }
            // if tmp file is specified and data does not fit in memory, write table1 to tmp file
            else if (rounds > 1)
            {
                // Write data to disk if required
                start_time_io = omp_get_wtime();

                // Seek to the correct position in the file
                off_t offset = r * num_records_in_bucket * total_num_buckets * sizeof(MemoRecord); // total_num_buckets that fit in memory
                if (fseeko(fd_tmp, offset, SEEK_SET) < 0)
                {
                    perror("Error seeking in file");
                    fclose(fd_tmp);
                    exit(EXIT_FAILURE);
                }

                size_t bytesWritten = 0;
                // Set the number of threads if specified
                if (num_threads_io > 0)
                {
                    omp_set_num_threads(num_threads);
                }

                // write table1
                for (unsigned long long i = 0; i < total_num_buckets; i++)
                {
                    size_t elements_written = fwrite(buckets[i].records, sizeof(MemoRecord), num_records_in_bucket, fd_tmp);
                    if (elements_written != num_records_in_bucket)
                    {
                        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                                elements_written, num_records_in_bucket);
                        fclose(fd_tmp);
                        exit(EXIT_FAILURE);
                    }
                    bytesWritten += elements_written * sizeof(MemoRecord);
                }

                for (unsigned long long i = 0; i < total_num_buckets; i++)
                {
                    memset(buckets[i].records, 0, num_records_in_bucket * sizeof(MemoRecord));
                }

                // End I/O time measurement
                end_time_io = omp_get_wtime();
                elapsed_time_io = end_time_io - start_time_io;
                elapsed_time_io_total += elapsed_time_io;
            }

            // Calculate throughput (hashes per second)
            throughput_hash = (num_records_per_round / (elapsed_time_hash + elapsed_time_io)) / (1e6);

            // Calculate I/O throughput
            if (rounds > 1)
            {
                throughput_io = (num_records_per_round * sizeof(MemoRecord)) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);
            }
            else
            {
                throughput_io = (num_records_per_round * sizeof(MemoTable2Record)) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);
            }

            // Check Table1 Efficiency for this round
            if (!BENCHMARK)
            {
                printf("[%.2f] HashGen %.2f%%: %.2f MH/s : I/O %.2f MB/s\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds, throughput_hash, throughput_io);
                if (VERIFY)
                {
                    unsigned long long num_zero = 0;
                    unsigned long long avg_num_records_in_bucket = 0;
                    for (unsigned long long i = 0; i < total_num_buckets; i++)
                    {
                        num_zero += num_records_in_bucket - buckets[i].count;
                        avg_num_records_in_bucket += buckets[i].count;
                    }
                    avg_num_records_in_bucket /= total_num_buckets;
                    printf("[%.2f] Table1 Storage efficiency : %.2f%%, Zero nonces : %llu / %llu, Avg num records in a bucket : %llu\n", omp_get_wtime() - start_time, 100 * (1 - ((double)num_zero / num_records_per_round)), num_zero, total_num_buckets * num_records_in_bucket, avg_num_records_in_bucket);
                }
            }
            // end of loop
        }

        start_time_io = omp_get_wtime();

        // Flush memory buffer to disk and close the file
        if (writeDataTmp)
        {
            if (fflush(fd_tmp) != 0)
            {
                perror("Failed to flush buffer");
                fclose(fd_tmp);
                return EXIT_FAILURE;
            }
            if (fsync(fileno(fd_tmp)) != 0)
            {
                perror("Failed to fsync buffer");
                fclose(fd_tmp);
                return EXIT_FAILURE;
            }
        }

        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        // should move timing for I/O to after this section of code

        // Free allocated memory
        for (unsigned long long i = 0; i < total_num_buckets; i++)
        {
            free(buckets[i].records);
            free(buckets2[i].records);

            buckets[i].records = NULL;
            buckets2[i].records = NULL;
        }
        free(buckets);
        free(buckets2);

        buckets = NULL;
        buckets2 = NULL;

        // out-of-memory
        // Extract buckets from table1, generate table2, shuffle table2, and write to disk (filename_table2)
        if (writeDataTmpTable2 && rounds > 1)
        {
            if (!BENCHMARK)
                printf("------------------------Table 2 generation started------------------------\n");

            // Shuffle table1 and write to disk
            FILE *fd_table2_tmp = NULL;
            fd_table2_tmp = fopen(FILENAME_TMP_TABLE2, "wb+");
            if (fd_table2_tmp == NULL)
            {
                printf("Error opening file %s (#6)\n", FILENAME_TMP_TABLE2);
                perror("Error opening file");
                return EXIT_FAILURE;
            }

            // memory_needed = num_buckets_to_read * r * num_records_in_bucket * sizeof(MemoRecord);
            unsigned long long num_diff_pref_buckets_to_read = MEMORY_SIZE_bytes / (rounds * num_records_in_bucket * sizeof(MemoRecord)); // the number of buckets of different prefixes to read
            if (total_num_buckets % num_buckets_to_read != 0)
            {
                printf("Warning: total_num_buckets=%llu is not a multiple of num_buckets_to_read=%llu, adjusting num_buckets_to_read...\n", total_num_buckets, num_buckets_to_read);
            }

            if (DEBUG)
                printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * sizeof(MemoRecord) * num_buckets_to_read);
            // need to fix this for 5 byte NONCE_SIZE
            // if (total_num_buckets % num_buckets_to_read != 0)
            // {
            //     printf("Warning: total_num_buckets=%llu is not a multiple of num_buckets_to_read=%llu, adjusting num_buckets_to_read...\n", total_num_buckets, num_buckets_to_read);
            //     uint64_t ratio = total_num_buckets / num_buckets_to_read;
            //     uint64_t result = largest_power_of_two_less_than(ratio);
            //     if (DEBUG)
            //         printf("Largest power of 2 less than %lu is %lu\n", ratio, result);
            //     num_buckets_to_read = total_num_buckets / result;
            //     printf("num_buckets_to_read (if num_buckets mod num_buckets_to_read != 0)=%llu\n", num_buckets_to_read);
            //     if (DEBUG)
            //         printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
            //     // printf("error, num_buckets_to_read is not a multiple of total_num_buckets, exiting: total_num_buckets=%llu num_buckets_to_read=%llu...\n",total_num_buckets,num_buckets_to_read);
            //     // return EXIT_FAILURE;
            // }

            rewind(fd_tmp);

            // Set the number of threads if specified
            if (num_threads > 0)
            {
                omp_set_num_threads(num_threads);
            }
            // unsigned long long num_total_buckets_to_read = num_diff_pref_buckets_to_read * rounds; // the number of how many buckets will be stored in bucket structure

            buckets = (Bucket *)calloc(num_diff_pref_buckets_to_read, sizeof(Bucket));
            for (unsigned long long i = 0; i < num_diff_pref_buckets_to_read; i++)
            {
                buckets[i].records = (MemoRecord *)calloc(num_records_in_shuffled_bucket, sizeof(MemoRecord));
                if (buckets[i].records == NULL)
                {
                    fprintf(stderr, "Error: Unable to allocate memory for records in shuffled bucket.\n");
                    fclose(fd_table2_tmp);
                    free(buckets);
                    return EXIT_FAILURE;
                }
            }

            // Allocate memory for the destination buckets
            buckets2 = (BucketTable2 *)calloc(total_num_buckets, sizeof(BucketTable2));
            if (buckets2 == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for buckets2.\n");
                exit(EXIT_FAILURE);
            }
            for (unsigned long long i = 0; i < total_num_buckets; i++)
            {
                buckets2[i].records = (MemoTable2Record *)calloc(num_records_in_bucket, sizeof(MemoTable2Record));
                if (buckets2[i].records == NULL)
                {
                    fprintf(stderr, "Error: Unable to allocate memory for records in table 2.\n");
                    exit(EXIT_FAILURE);
                }
            }

            for (unsigned long long r = 0; r < rounds; r++)
            {
                for (unsigned long long b = 0; b < num_diff_pref_buckets_to_read; b++)
                {
                    buckets[b].count = 0;
                    buckets[b].count_waste = 0;
                    buckets[b].full = false;
                    buckets[b].flush = 0;
                }

                for (unsigned long long b = 0; b < num_diff_pref_buckets_to_read; b++)
                {
                    off_t offset_src = (r * total_num_buckets + b) * num_records_in_bucket * sizeof(MemoRecord);
                    if (fseeko(fd_tmp, offset_src, SEEK_SET) != 0)
                    {
                        perror("fseeko failed");
                        fclose(fd_tmp);
                        return EXIT_FAILURE;
                    }

                    size_t elements_read = fread(buckets[b].records + buckets[b].count, sizeof(MemoRecord), num_records_in_bucket, fd_tmp);
                    if (elements_read != num_records_in_bucket)
                    {
                        fprintf(stderr, "Error reading from file; elements read %zu when expected %llu\n",
                                elements_read, num_records_in_bucket);
                        fclose(fd_table2_tmp);
                        free(buckets);
                        free(buckets2);
                        return EXIT_FAILURE;
                    }
                    buckets[b].count += elements_read;
                }

#pragma omp parallel for schedule(static)
                for (unsigned long long b = 0; b < num_diff_pref_buckets_to_read; b++)
                {
                    sort_bucket_records_inplace(buckets[b].records, num_records_in_bucket);
                    generate_table2(buckets[b].records, num_records_in_bucket);
                }

                for (unsigned long long b = 0; b < total_num_buckets; b++)
                {
                    size_t elements_written = fwrite(buckets2[b].records, sizeof(MemoTable2Record), num_records_in_bucket, fd_table2_tmp);
                    if (elements_written != num_records_in_bucket)
                    {
                        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                                elements_written, num_records_in_bucket);
                        fclose(fd_table2_tmp);
                        free(buckets);
                        free(buckets2);
                        return EXIT_FAILURE;
                    }
                }

                printf("[%.2f] Table2Gen %.2f%%\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds);
            }

            for (unsigned long long i = 0; i < num_buckets_to_read * rounds; i++)
            {
                free(buckets[i].records);
            }
            for (unsigned long long i = 0; i < total_num_buckets; i++)
            {
                free(buckets2[i].records);
            }

            free(buckets);
            free(buckets2);

            // end of for loop
            start_time_io = omp_get_wtime();

            // Flush and close the file
            if (writeDataTmp)
            {
                if (fflush(fd_tmp) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd_tmp);
                    return EXIT_FAILURE;
                }

                if (fsync(fileno(fd_tmp)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd_tmp);
                    return EXIT_FAILURE;
                }
                fclose(fd_tmp);
            }

            // flush the destination file, don't close it, we will use it in the next step when generating table2
            if (fflush(fd_table2_tmp) != 0)
            {
                perror("Failed to flush buffer");
                fclose(fd_table2_tmp);
                return EXIT_FAILURE;
            }
            if (fsync(fileno(fd_table2_tmp)) != 0)
            {
                perror("Failed to fsync buffer");
                fclose(fd_table2_tmp);
                return EXIT_FAILURE;
            }

            remove_file(FILENAME_TMP);

            end_time_io = omp_get_wtime();
            elapsed_time_io = end_time_io - start_time_io;
            elapsed_time_io_total += elapsed_time_io;

            // Shuffle Table2 and write to disk
            if (writeDataTable2)
            {
                if (!BENCHMARK)
                    printf("------------------------Table 2 shuffling started------------------------\n");

                // Open the file for writing shuffled table2
                FILE *fd_table2 = NULL;
                fd_table2 = fopen(FILENAME_TABLE2, "wb+");
                if (fd_table2 == NULL)
                {
                    printf("Error opening file %s (#5)\n", FILENAME_TABLE2);
                    perror("Error opening file");
                    return EXIT_FAILURE;
                }

                num_buckets_to_read = ceil((MEMORY_SIZE_bytes / (num_records_in_bucket * sizeof(MemoTable2Record))) / 2);
                if (DEBUG)
                    printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * sizeof(MemoTable2Record) * num_buckets_to_read);

                // need to fix this for 5 byte NONCE_SIZE
                if (total_num_buckets % num_buckets_to_read != 0)
                {
                    uint64_t ratio = total_num_buckets / num_buckets_to_read;
                    uint64_t result = largest_power_of_two_less_than(ratio);
                    if (DEBUG)
                        printf("Largest power of 2 less than %lu is %lu\n", ratio, result);
                    num_buckets_to_read = total_num_buckets / result;
                    printf("num_buckets_to_read (if num_buckets mod num_buckets_to_read != 0)=%llu\n", num_buckets_to_read);
                    if (DEBUG)
                        printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * sizeof(MemoTable2Record) * num_buckets_to_read);
                    // printf("error, num_buckets_to_read is not a multiple of total_num_buckets, exiting: num_buckets=%llu num_buckets_to_read=%llu...\n",total_num_buckets,num_buckets_to_read);
                    // return EXIT_FAILURE;
                }

                // Calculate the total number of records to read per batch
                size_t records_per_batch = num_records_in_bucket * num_buckets_to_read;
                size_t buffer_size = records_per_batch * rounds;

                if (num_threads_io > 0)
                {
                    omp_set_num_threads(num_threads_io);
                }

                shuffle_table2(fd_table2_tmp, fd_table2, buffer_size, records_per_batch, num_buckets_to_read, start_time, elapsed_time_io2, elapsed_time_io2_total);

                // Flush and close the file
                if (writeDataTmpTable2)
                {
                    if (fflush(fd_table2_tmp) != 0)
                    {
                        perror("Failed to flush buffer");
                        fclose(fd_table2_tmp);
                        return EXIT_FAILURE;
                    }

                    if (fsync(fileno(fd_table2_tmp)) != 0)
                    {
                        perror("Failed to fsync buffer");
                        fclose(fd_table2_tmp);
                        return EXIT_FAILURE;
                    }
                    fclose(fd_table2_tmp);
                }

                if (fflush(fd_table2) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd_table2);
                    return EXIT_FAILURE;
                }
                if (fsync(fileno(fd_table2)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd_table2);
                    return EXIT_FAILURE;
                }

                fclose(fd_table2);
                remove_file(FILENAME_TMP_TABLE2);
            }
        }
        else if (writeDataTable2 && rounds == 1)
        {
            // Call the rename_file function
            if (move_file_overwrite(FILENAME_TMP, FILENAME_TABLE2) == 0)
            {
                if (!BENCHMARK)
                    printf("File renamed/moved successfully from '%s' to '%s'.\n", FILENAME_TMP, FILENAME_TABLE2);
            }
            else
            {
                printf("Error in moving file '%s' to '%s'.\n", FILENAME_TMP, FILENAME_TABLE2);
                return EXIT_FAILURE;
                // Error message already printed by rename_file via perror()
                // Additional handling can be done here if necessary
                // return 1;
            }
        }

        // will need to check on MacOS with a spinning hdd if we need to call sync() to flush all filesystems
#ifdef __linux__
        if (writeDataTable2)
        {
            if (DEBUG)
                printf("Final flush in progress...\n");
            int fd2 = open(FILENAME_TABLE2, O_RDWR);
            if (fd2 == -1)
            {
                printf("Error opening file %s (#6)\n", FILENAME_TABLE2);

                perror("Error opening file");
                return EXIT_FAILURE;
            }

            // Sync the entire filesystem
            if (syncfs(fd2) == -1)
            {
                perror("Error syncing filesystem with syncfs");
                close(fd2);
                return EXIT_FAILURE;
            }
        }
#endif
        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        // End total time measurement
        double end_time = omp_get_wtime();
        double elapsed_time = end_time - start_time;

        // Calculate total throughput
        double total_throughput = (num_records_total / elapsed_time) / 1e6;
        if (!BENCHMARK)
        {
            printf("Total Throughput: %.2f MH/s  %.2f MB/s\n", total_throughput, total_throughput * NONCE_SIZE);
            printf("Total Time: %.6f seconds\n", elapsed_time);
        }
        else
        {
            printf("%s %d %lu %d %llu %.2f %zu %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", approach, K, sizeof(MemoRecord), num_threads, MEMORY_SIZE_MB, file_size_gb, BATCH_SIZE, total_throughput, total_throughput * NONCE_SIZE, elapsed_time_hash_total, elapsed_time_io_total, elapsed_time_io2_total, elapsed_time - elapsed_time_hash_total - elapsed_time_io_total - elapsed_time_io2_total, elapsed_time);
            return 0;
        }
    }
    // end of HASHGEN

    // omp_set_num_threads(num_threads);

    if (SEARCH && !SEARCH_BATCH)
    {
        search_memo_records(FILENAME_TABLE2, SEARCH_STRING);
    }

    if (SEARCH_BATCH)
    {
        search_memo_records_batch(FILENAME_TABLE2, BATCH_SIZE, PREFIX_SEARCH_SIZE);
    }

    // Call the function to count zero-value MemoRecords
    // printf("verifying efficiency of final stored file...\n");
    // count_zero_memo_records(FILENAME_FINAL);

    // Call the function to process MemoRecords
    if (VERIFY)
    {
        if (!BENCHMARK)
            printf("------------------------Verifying started------------------------\n");

        // process_memo_records(FILENAME_FINAL, MEMORY_SIZE_bytes / sizeof(MemoRecord));
        process_memo_records_table2(FILENAME_TABLE2, num_records_in_bucket * rounds);
    }

    if (DEBUG)
        printf("SUCCESS!\n");
    return 0;
}
