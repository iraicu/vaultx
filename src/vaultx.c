#include "vaultx.h"

// Function to display usage information
void print_usage(char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -a, --approach [task|for]    Select parallelization approach (default: for)\n");
    printf("  -t, --threads NUM            Number of threads to use (default: number of available cores)\n");
    printf("  -i, --threads_io NUM         Number of I/O threads (default: number of available cores)\n");
    printf("  -K, --exponent NUM           Exponent K to compute 2^K number of records (default: 4)\n");
    printf("  -m, --memory NUM             Memory size in MB (default: 1)\n");
    printf("  -f, --file NAME              Temporary file name\n");
    printf("  -g, --final file NAME        Final file name\n");
    printf("  -b, --batch-size NUM         Batch size (default: 1024)\n");
    printf("  -2, --table2                 Use Table2 approach (should specify -f (table1 file), if table1 was created previously, turn off HASHGEN)\n");
    printf("  -M, --match THRESHOLD        Specify the threshold to match hashes against in table2 creation (default: 3)\n");
    printf("  -s, --search STRING          Search for a specific hash prefix in the file\n");
    printf("  -S, --search-batch NUM       Search for a specific hash prefix in the file in batch mode\n");
    printf("  -x, --benchmark              Enable benchmark mode (default: false)\n");
    printf("  -h, --help                   Display this help message\n");
    printf("\nExample:\n");
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo\n", prog_name);
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -x true (Only prints time)\n", prog_name);
    printf("  %s -a for -t 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -2 true\n", prog_name);
}

// Function to compute the bucket index based on hash prefix
off_t getBucketIndex(const uint8_t *hash, size_t prefix_size)
{
    off_t index = 0;
    for (size_t i = 0; i < prefix_size && i < HASH_SIZE; i++)
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

// Function to generate Blake3 hash
void generateBlake3(uint8_t *record_hash, MemoRecord *record, unsigned long long seed)
{
    // Ensure that the pointers are valid
    if (record_hash == NULL)
    {
        fprintf(stderr, "Error: NULL pointer passed to generateBlake3.\n");
        return;
    }

    // Store seed into the nonce
    memcpy(record->nonce, &seed, NONCE_SIZE);

    // Generate Blake3 hash
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, record_hash, HASH_SIZE);
}

// Comparator function to sort based on hash values (Blake3 hashes)
int compare_by_hash(const void *a, const void *b)
{
    MemoRecord *record_a = (MemoRecord *)a;
    MemoRecord *record_b = (MemoRecord *)b;

    uint8_t hash_a[HASH_SIZE];
    uint8_t hash_b[HASH_SIZE];

    // Generate hashes for both records' nonces
    generateBlake3(hash_a, record_a, 0);
    generateBlake3(hash_b, record_b, 0);

    // Compare the hashes lexicographically
    return memcmp(hash_a, hash_b, HASH_SIZE);
}

// Function to write a bucket of records to disk sequentially
size_t writeBucketToDiskSequential(const Bucket *bucket, FILE *fd)
{
    // printf("num_records_in_bucket=%llu sizeof(MemoRecord)=%d\n",num_records_in_bucket,sizeof(MemoRecord));
    size_t elementsWritten = fwrite(bucket->records, sizeof(MemoRecord), num_records_in_bucket, fd);
    if (elementsWritten != num_records_in_bucket)
    {
        fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                elementsWritten, num_records_in_bucket);
        fclose(fd);
        exit(EXIT_FAILURE);
    }
    return elementsWritten * sizeof(MemoRecord);
}

// Function to insert a record into a bucket
void insert_record(Bucket *buckets, MemoRecord *record, size_t bucketIndex)
{
    if (bucketIndex >= num_buckets)
    {
        fprintf(stderr, "Error: Bucket index %zu out of range (0 to %llu).\n", bucketIndex, num_buckets - 1);
        return;
    }

    Bucket *bucket = &buckets[bucketIndex];
    size_t idx;

#pragma omp atomic capture
    {
        idx = bucket->count;
        bucket->count++;
    }

    // Check if there's room in the bucket
    if (idx < num_records_in_bucket)
    {
        memcpy(bucket->records[idx].nonce, record->nonce, NONCE_SIZE);
    }
    else
    {
        // Bucket is full; handle overflow if necessary
        // For now, we ignore overflow
        bucket->count = num_records_in_bucket;
        if (!bucket->full)
        {
#pragma omp atomic
            full_buckets_global++;
            bucket->full = true;
        }
        bucket->count_waste++;
    }
}

// Function to concatenate two strings and return the result
char *concat_strings(const char *str1, const char *str2)
{
    // Check for NULL pointers
    if (str1 == NULL || str2 == NULL)
    {
        fprintf(stderr, "Error: NULL string passed to concat_strings.\n");
        return NULL;
    }

    // Calculate the lengths of the input strings
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    // Allocate memory for the concatenated string (+1 for the null terminator)
    char *result = (char *)malloc(len1 + len2 + 1);
    if (result == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed in concat_strings.\n");
        return NULL;
    }

    // Copy the first string into the result
    strcpy(result, str1);

    // Append the second string to the result
    strcat(result, str2);

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

// Function to count zero-value MemoRecords in a binary file
size_t count_zero_memo_records(const char *filename)
{
    const size_t BATCH_SIZE = 1000000; // 1 million MemoRecords per batch
    MemoRecord *buffer = NULL;
    size_t total_zero_records = 0;
    size_t total_nonzero_records = 0;
    size_t records_read;
    FILE *file = NULL;

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {

        printf("Error opening file %s (#1)\n", filename);
        return 0;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(BATCH_SIZE * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        // Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            // Check if the MemoRecord's nonce is all zeros
            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                ++total_nonzero_records;
            }
            else
            {
                ++total_zero_records;
            }
        }
    }

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of zero-value MemoRecords
    printf("total_zero_records=%zu total_nonzero_records=%zu efficiency=%.2f%%\n", total_zero_records, total_nonzero_records, total_nonzero_records * 100.0 / (total_zero_records + total_nonzero_records));

    return total_zero_records;
}

long get_file_size(const char *filename)
{
    FILE *file = fopen(filename, "rb"); // Open the file in binary mode
    long size;

    if (file == NULL)
    {
        printf("Error opening file %s (#2)\n", filename);

        perror("Error opening file");
        return -1;
    }

    // Move the file pointer to the end of the file
    if (fseek(file, 0, SEEK_END) != 0)
    {
        perror("Error seeking to end of file");
        fclose(file);
        return -1;
    }

    // Get the current position in the file, which is the size
    size = ftell(file);
    if (size == -1L)
    {
        perror("Error getting file position");
        fclose(file);
        return -1;
    }

    fclose(file);
    return size;
}

size_t process_memo_records(const char *filename, const size_t BATCH_SIZE)
{
    MemoRecord *buffer = NULL;
    size_t total_records = 0;
    size_t zero_nonce_count = 0;
    size_t records_read;
    FILE *file = NULL;
    uint8_t prev_hash[PREFIX_SIZE] = {0}; // Initialize previous hash prefix to zero
    uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    size_t count_condition_met = 0;       // Counter for records meeting the condition
    size_t count_condition_not_met = 0;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return 0;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(BATCH_SIZE * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return 0;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    // Read the file in batches
    while ((records_read = fread(buffer, sizeof(MemoRecord), BATCH_SIZE, file)) > 0)
    {
        double start_time_verify = omp_get_wtime();
        double end_time_verify = omp_get_wtime();

        // Process each MemoRecord in the batch
        for (size_t i = 0; i < records_read; ++i)
        {
            ++total_records;

            if (is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
            {
                uint8_t hash_output[HASH_SIZE];

                // Compute Blake3 hash of the nonce
                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, buffer[i].nonce, NONCE_SIZE);
                blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);

                // Compare the first PREFIX_SIZE bytes of the current hash to the previous hash prefix
                if (memcmp(hash_output, prev_hash, PREFIX_SIZE) >= 0)
                {
                    // Current hash's first PREFIX_SIZE bytes are equal to or greater than previous
                    ++count_condition_met;
                }
                else
                {
                    ++count_condition_not_met;

                    if (DEBUG)
                    {
                        // Print previous hash and nonce, and current hash and nonce
                        printf("Condition not met at record %zu:\n", total_records);
                        printf("Previous nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", prev_nonce[n]);
                        printf("\n");
                        printf("Previous hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", prev_hash[n]);
                        printf("\n");

                        printf("Current nonce: ");
                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce[n]);
                        printf("\n");
                        printf("Current hash prefix: ");
                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", hash_output[n]);
                        printf("\n");
                    }
                }

                // Update the previous hash prefix and nonce
                memcpy(prev_hash, hash_output, PREFIX_SIZE);
                memcpy(prev_nonce, buffer[i].nonce, NONCE_SIZE);
            }
            else
            {
                ++zero_nonce_count;
                // Optionally, handle zero nonces here
            }
        }
        end_time_verify = omp_get_wtime();
        double elapsed_time_verify = end_time_verify - start_time_verify;
        double elapsed_time = omp_get_wtime() - start_time;

        // Calculate throughput (hashes per second)
        double throughput = (BATCH_SIZE * sizeof(MemoRecord) / elapsed_time_verify) / (1024 * 1024);
        printf("[%.2f] Verify %.2f%%: %.2f MB/s\n", elapsed_time, total_records * sizeof(MemoRecord) * 100.0 / filesize, throughput);
    }

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    printf("sorted=%zu not_sorted=%zu zero_nonces=%zu total_records=%zu storage_efficiency=%.2f%%\n",
           count_condition_met, count_condition_not_met, zero_nonce_count, total_records, count_condition_met * 100.0 / total_records);

    return count_condition_met;
}

/**
 * Converts a given string to an array of uint8_t.
 *
 * @param SEARCH_STRING The input string to convert.
 * @return A pointer to the array of uint8_t, or NULL if allocation fails.
 */
uint8_t *convert_string_to_uint8_array(const char *SEARCH_STRING)
{
    if (SEARCH_STRING == NULL)
    {
        return NULL;
    }

    size_t length = strlen(SEARCH_STRING);
    uint8_t *array = (uint8_t *)malloc(length * sizeof(uint8_t));
    if (array == NULL)
    {
        // Memory allocation failed
        return NULL;
    }

    for (size_t i = 0; i < length; ++i)
    {
        array[i] = (uint8_t)SEARCH_STRING[i];
    }

    return array;
}

uint8_t *hexStringToByteArray(const char *hexString)
{

    size_t hexLen = strlen(hexString);
    uint8_t *byteArray = (uint8_t *)malloc(hexLen * sizeof(uint8_t));
    // size_t hexLen = strlen(hexString);
    if (hexLen % 2 != 0)
    {
        return NULL; // Error: Invalid hexadecimal string length
    }

    size_t byteLen = hexLen / 2;
    size_t byteArraySize = byteLen;
    if (byteLen > byteArraySize)
    {
        return NULL; // Error: Byte array too small
    }

    for (size_t i = 0; i < byteLen; ++i)
    {
        if (sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]) != 1)
        {
            return NULL; // Error: Failed to parse hexadecimal string
        }
    }

    return byteArray;
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

// Recursive function to generate numbers using divide and conquer
void generateHashes(unsigned long long start, unsigned long long end)
{
    if (end - start < BATCH_SIZE)
    {
        // printf("%d\n", start); // Print a single number

        MemoRecord record;
        uint8_t record_hash[HASH_SIZE];

        // unsigned long long batch_end = i + BATCH_SIZE;
        // if (batch_end > end_idx) {
        //     batch_end = end_idx;
        // }

        for (unsigned long long j = start; j <= end; j++)
        {
            generateBlake3(record_hash, &record, j);
            if (MEMORY_WRITE)
            {
                off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                insert_record(buckets, &record, bucketIndex);
            }
        }

        return;
    }

    unsigned long long mid = (start + end) / 2;

// Recursive calls for the left and right halves
#pragma omp task shared(start, mid) // Task for the left half
    generateHashes(start, mid);     // Generate numbers in the left half

#pragma omp task shared(mid, end) // Task for the right half
    generateHashes(mid + 1, end); // Generate numbers in the right half

#pragma omp taskwait // Wait for both tasks to complete
}

int main(int argc, char *argv[])
{
    // Default values
    const char *approach = "for"; // Default approach
    int num_threads = 0;          // 0 means OpenMP chooses
    int num_threads_io = 0;
    int K = 4;                                     // Default exponent
    unsigned long long num_iterations = 1ULL << K; // 2^K iterations
    unsigned long long num_hashes = num_iterations;
    unsigned long long MEMORY_SIZE_MB = 1;
    // unsigned long long MEMORY_SIZE_bytes_original = 0;
    char *FILENAME = NULL;       // Default output file name
    char *FILENAME_FINAL = NULL; // Default output file name
    char *FILENAME_TABLE2 = NULL;
    char *SEARCH_STRING = NULL; // Default output file name

    // Define long options
    static struct option long_options[] = {
        {"approach", required_argument, 0, 'a'},
        {"threads", required_argument, 0, 't'},
        {"threads_io", required_argument, 0, 'i'},
        {"exponent", required_argument, 0, 'K'},
        {"memory", required_argument, 0, 'm'},
        {"file", required_argument, 0, 'f'},
        {"file_final", required_argument, 0, 'g'},
        {"file_table2", required_argument, 0, '2'},
        {"batch_size", required_argument, 0, 'b'},
        {"memory_write", required_argument, 0, 'w'},
        {"circular_array", required_argument, 0, 'c'},
        {"verify", required_argument, 0, 'v'},
        {"search", required_argument, 0, 's'},
        {"prefix_search_size", required_argument, 0, 'S'},
        {"benchmark", required_argument, 0, 'x'},
        {"debug", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "a:t:i:K:m:f:g:b:w:c:2:v:s:S:x:d:h", long_options, &option_index)) != -1)
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
            num_iterations = 1ULL << K; // Compute 2^K
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
            FILENAME = optarg;
            writeData = true;
            break;
        case 'g':
            FILENAME_FINAL = optarg;
            writeDataFinal = true;
            break;
        case '2':
            FILENAME_TABLE2 = optarg;
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

    if ((SEARCH || SEARCH_BATCH) && !FILENAME_FINAL)
    {
        fprintf(stderr, "Error: Final file name (-g) is required for search operations.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set the number of threads if specified
    if (num_threads > 0)
    {
        omp_set_num_threads(num_threads);
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

    unsigned long long file_size_bytes = num_iterations * NONCE_SIZE;
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
    num_hashes = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    MEMORY_SIZE_bytes = num_hashes * NONCE_SIZE;
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);

    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));

    num_hashes = MEMORY_SIZE_bytes / NONCE_SIZE;

    num_buckets = 1ULL << (PREFIX_SIZE * 8);

    num_records_in_bucket = num_hashes / num_buckets;

    MEMORY_SIZE_bytes = num_buckets * num_records_in_bucket * sizeof(MemoRecord);
    MEMORY_SIZE_MB = (unsigned long long)(MEMORY_SIZE_bytes / (1024 * 1024));
    file_size_bytes = MEMORY_SIZE_bytes * rounds;
    file_size_gb = file_size_bytes / (1024 * 1024 * 1024.0);
    num_hashes = floor(MEMORY_SIZE_bytes / NONCE_SIZE);
    num_iterations = num_hashes * rounds;

    if (!BENCHMARK)
    {
        if (SEARCH)
        {
            printf("SEARCH                      : true\n");
        }
        else
        {
            printf("File Size (GB)              : %.2f\n", file_size_gb);
            printf("File Size (bytes)           : %llu\n", file_size_bytes);

            printf("Memory Size (MB)            : %llu\n", MEMORY_SIZE_MB);
            printf("Memory Size (bytes)         : %llu\n", MEMORY_SIZE_bytes);

            printf("Number of Hashes (RAM)      : %llu\n", num_hashes);

            printf("Number of Hashes (Disk)     : %llu\n", num_iterations);
            printf("Size of MemoRecord          : %lu\n", sizeof(MemoRecord));
            printf("Rounds                      : %llu\n", rounds);

            printf("Number of Buckets           : %llu\n", num_buckets);
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

            if (writeData)
            {
                printf("Temporary File              : %s\n", FILENAME);
            }
            if (writeDataFinal)
            {
                printf("Output File Final           : %s\n", FILENAME_FINAL);
            }
            if (writeDataTable2)
            {
                printf("Output File Table2          : %s\n", FILENAME_TABLE2);
            }
        }
    }

    if (HASHGEN)
    {
        // Open the file for writing in binary mode
        FILE *fd = NULL;
        if (writeData)
        {
            fd = fopen(FILENAME, "wb+");
            if (fd == NULL)
            {
                printf("Error opening file %s (#4)\n", FILENAME);

                perror("Error opening file");
                return EXIT_FAILURE;
            }
        }

        // Start walltime measurement
        double start_time = omp_get_wtime();

        // Allocate memory for the array of Buckets
        buckets = (Bucket *)calloc(num_buckets, sizeof(Bucket));
        if (buckets == NULL)
        {
            fprintf(stderr, "Error: Unable to allocate memory for buckets.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate memory for each bucket's records
        for (unsigned long long i = 0; i < num_buckets; i++)
        {
            buckets[i].records = (MemoRecord *)calloc(num_records_in_bucket, sizeof(MemoRecord));
            if (buckets[i].records == NULL)
            {
                fprintf(stderr, "Error: Unable to allocate memory for records.\n");
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

        for (unsigned long long r = 0; r < rounds; r++)
        {
            start_time_hash = omp_get_wtime();

            // Reset bucket counts
            for (unsigned long long i = 0; i < num_buckets; i++)
            {
                buckets[i].count = 0;
                buckets[i].count_waste = 0;
                buckets[i].full = false;
                buckets[i].flush = 0;
            }

            unsigned long long MAX_NUM_HASHES = 1ULL << (NONCE_SIZE * 8);

            unsigned long long start_idx = r * num_hashes;
            unsigned long long end_idx = start_idx + num_hashes;
            unsigned long long nonce_max = 0;

            printf("MAX_NUM_HASHES=%llu rounds=%llu num_hashes=%llu start_idx = %llu, end_idx = %llu\n", MAX_NUM_HASHES, rounds, num_hashes, start_idx, end_idx);

            // Parallel region based on selected approach
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
                                        off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
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
                volatile int cancel_flag = 0; // Shared flag
                full_buckets_global = 0;
// Parallel For Loop Approach
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
                            off_t bucketIndex = getBucketIndex(record_hash, PREFIX_SIZE);
                            insert_record(buckets, &record, bucketIndex);
                        }
                    }

                    // Set the flag if the termination condition is met.
                    // if (i > end_idx/2 && full_buckets_global == num_buckets) {
                    if (full_buckets_global >= num_buckets)
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

            // Write data to disk if required
            if (writeData)
            {
                start_time_io = omp_get_wtime();

                // Seek to the correct position in the file
                off_t offset = r * num_records_in_bucket * num_buckets * NONCE_SIZE; // num_buckets that fit in memory
                if (fseeko(fd, offset, SEEK_SET) < 0)
                {
                    perror("Error seeking in file");
                    fclose(fd);
                    exit(EXIT_FAILURE);
                }

                size_t bytesWritten = 0;
                // Set the number of threads if specified
                if (num_threads_io > 0)
                {
                    omp_set_num_threads(num_threads_io);
                }

// Write buckets to disk
#pragma omp parallel for schedule(static)
                for (unsigned long long i = 0; i < num_buckets; i++)
                {
                    bytesWritten += writeBucketToDiskSequential(&buckets[i], fd);
                }

                // End I/O time measurement
                end_time_io = omp_get_wtime();
                elapsed_time_io = end_time_io - start_time_io;
                elapsed_time_io_total += elapsed_time_io;

                // count how many full buckets , this works for rounds == 1
                // if (rounds == 1)
                //{
                unsigned long long full_buckets = 0;
                unsigned long long record_counts = 0;
                unsigned long long record_counts_waste = 0;
                for (unsigned long long i = 0; i < num_buckets; i++)
                {
                    if (buckets[i].count == num_records_in_bucket)
                        full_buckets++;
                    record_counts += buckets[i].count;
                    record_counts_waste += buckets[i].count_waste;
                }

                printf("record_counts=%llu storage_efficiency=%.2f full_buckets=%llu bucket_efficiency=%.2f nonce_max=%llu record_counts_waste=%llu hash_efficiency=%.2f\n", record_counts, record_counts * 100.0 / (num_buckets * num_records_in_bucket), full_buckets, full_buckets * 100.0 / num_buckets, nonce_max, record_counts_waste, num_buckets * num_records_in_bucket * 100.0 / (record_counts_waste + num_buckets * num_records_in_bucket));
            }

            // Calculate throughput (hashes per second)
            throughput_hash = (num_hashes / (elapsed_time_hash + elapsed_time_io)) / (1e6);

            // Calculate I/O throughput
            throughput_io = (num_hashes * NONCE_SIZE) / ((elapsed_time_hash + elapsed_time_io) * 1024 * 1024);

            if (!BENCHMARK)
                printf("[%.2f] HashGen %.2f%%: %.2f MH/s : I/O %.2f MB/s\n", omp_get_wtime() - start_time, (r + 1) * 100.0 / rounds, throughput_hash, throughput_io);
            // end of loop
        }

        start_time_io = omp_get_wtime();

        // Flush memory buffer to disk and close the file
        if (writeData)
        {
            if (fflush(fd) != 0)
            {
                perror("Failed to flush buffer");
                fclose(fd);
                return EXIT_FAILURE;
            }
        }

        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        // should move timing for I/O to after this section of code

        /*
        // Verification if enabled
        if (VERIFY) {
            unsigned long long num_zero = 0;
            for (unsigned long long i = 0; i < num_buckets; i++) {
                for (unsigned long long j = 0; j < buckets[i].count; j++) {
                    if (byteArrayToLongLong(buckets[i].records[j].nonce, NONCE_SIZE) == 0)
                        num_zero++;
                }
            }
            printf("Number of zero nonces: %llu\n", num_zero);
        }*/

        // Free allocated memory
        for (unsigned long long i = 0; i < num_buckets; i++)
        {
            free(buckets[i].records);
        }
        free(buckets);

        if (writeDataFinal && rounds > 1)
        {
            // Open the file for writing in binary mode
            FILE *fd_dest = NULL;
            fd_dest = fopen(FILENAME_FINAL, "wb+");
            if (fd_dest == NULL)
            {
                printf("Error opening file %s (#5)\n", FILENAME_FINAL);
                perror("Error opening file");

                // perror("Error opening file");
                return EXIT_FAILURE;
            }

            num_buckets_to_read = ceil((MEMORY_SIZE_bytes / (num_records_in_bucket * rounds * NONCE_SIZE)) / 2);
            printf("num_buckets_to_read=%llu\n", num_buckets_to_read);
            if (DEBUG)
                printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
            // need to fix this for 5 byte NONCE_SIZE
            if (num_buckets % num_buckets_to_read != 0)
            {
                uint64_t ratio = num_buckets / num_buckets_to_read;
                uint64_t result = largest_power_of_two_less_than(ratio);
                if (DEBUG)
                    printf("Largest power of 2 less than %lu is %lu\n", ratio, result);
                num_buckets_to_read = num_buckets / result;
                printf("num_buckets_to_read (if num_buckets mod num_buckets_to_read != 0)=%llu\n", num_buckets_to_read);
                if (DEBUG)
                    printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
                // printf("error, num_buckets_to_read is not a multiple of num_buckets, exiting: num_buckets=%llu num_buckets_to_read=%llu...\n",num_buckets,num_buckets_to_read);
                // return EXIT_FAILURE;
            }

            // Calculate the total number of records to read per batch
            size_t records_per_batch = num_records_in_bucket * num_buckets_to_read;
            // Calculate the size of the buffer needed
            size_t buffer_size = records_per_batch * rounds;

            // Allocate the buffer
            if (DEBUG)
                printf("allocating %lu bytes for buffer\n", buffer_size * sizeof(MemoRecord));
            MemoRecord *buffer = (MemoRecord *)malloc(buffer_size * sizeof(MemoRecord));
            if (buffer == NULL)
            {
                fprintf(stderr, "Error allocating memory for buffer.\n");
                exit(EXIT_FAILURE);
            }

            if (DEBUG)
                printf("allocating %lu bytes for bufferShuffled\n", buffer_size * sizeof(MemoRecord));
            MemoRecord *bufferShuffled = (MemoRecord *)malloc(buffer_size * sizeof(MemoRecord));
            if (bufferShuffled == NULL)
            {
                fprintf(stderr, "Error allocating memory for bufferShuffled.\n");
                exit(EXIT_FAILURE);
            }

            // Set the number of threads if specified
            if (num_threads_io > 0)
            {
                omp_set_num_threads(num_threads_io);
            }

            for (unsigned long long i = 0; i < num_buckets; i = i + num_buckets_to_read)
            {
                double start_time_io2 = omp_get_wtime();

#pragma omp parallel for schedule(static)
                for (unsigned long long r = 0; r < rounds; r++)
                {
                    //  Calculate the source offset
                    off_t offset_src = ((r * num_buckets + i) * num_records_in_bucket) * sizeof(MemoRecord);
                    if (DEBUG)
                        printf("read data: offset_src=%lu bytes=%lu\n",
                               offset_src, records_per_batch * sizeof(MemoRecord));

                    if (fseeko(fd, offset_src, SEEK_SET) < 0)
                    {
                        perror("Error seeking in file");
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }

                    // size_t recordsRead = fread(buffer+num_records_in_bucket*num_buckets_to_read*sizeof(MemoRecord)*r, sizeof(MemoRecord), num_records_in_bucket*num_buckets_to_read, fd);
                    //  Correct pointer arithmetic
                    size_t index = r * records_per_batch;
                    if (DEBUG)
                        printf("storing read data at index %lu\n", index);
                    size_t recordsRead = fread(&buffer[index],
                                               sizeof(MemoRecord),
                                               records_per_batch,
                                               fd);

                    if (recordsRead != records_per_batch)
                    {
                        fprintf(stderr, "Error reading file, records read %zu instead of %zu\n",
                                recordsRead, records_per_batch);
                        fclose(fd);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        if (DEBUG)
                            printf("read %zu records from disk...\n", recordsRead);
                    }

                    off_t offset_dest = i * num_records_in_bucket * NONCE_SIZE * rounds;
                    if (DEBUG)
                        printf("write data: offset_dest=%lu bytes=%llu\n", offset_dest, num_records_in_bucket * NONCE_SIZE * rounds * num_buckets_to_read);

                    if (fseeko(fd_dest, offset_dest, SEEK_SET) < 0)
                    {
                        perror("Error seeking in file");
                        fclose(fd_dest);
                        exit(EXIT_FAILURE);
                    }
                    // needs to make sure its ok, fix things....
                    // printf("buffer_size=%llu my_buffer_size=%llu\n",buffer_size,num_records_in_bucket*num_buckets_to_read*rounds);
                }
                // end of for loop rounds

                if (DEBUG)
                    printf("shuffling %llu buckets with %llu bytes each...\n", num_buckets_to_read * rounds, num_records_in_bucket * NONCE_SIZE);
#pragma omp parallel for schedule(static)
                for (unsigned long long s = 0; s < num_buckets_to_read; s++)
                {
                    for (unsigned long long r = 0; r < rounds; r++)
                    {
                        off_t index_src = ((r * num_buckets_to_read + s) * num_records_in_bucket);
                        off_t index_dest = (s * rounds + r) * num_records_in_bucket;

                        memcpy(&bufferShuffled[index_dest], &buffer[index_src], num_records_in_bucket * sizeof(MemoRecord));
                    }
                }
                // end of for loop num_buckets_to_read

                // should write in parallel if possible
                size_t elementsWritten = fwrite(bufferShuffled, sizeof(MemoRecord), num_records_in_bucket * num_buckets_to_read * rounds, fd_dest);
                if (elementsWritten != num_records_in_bucket * num_buckets_to_read * rounds)
                {
                    fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                            elementsWritten, num_records_in_bucket * num_buckets_to_read * rounds);
                    fclose(fd_dest);
                    exit(EXIT_FAILURE);
                }

                /*if (fsync(fileno(fd_dest)) != 0) {
                    perror("Failed to fsync buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }*/

                double end_time_io2 = omp_get_wtime();
                elapsed_time_io2 = end_time_io2 - start_time_io2;
                elapsed_time_io2_total += elapsed_time_io2;
                double throughput_io2 = (num_records_in_bucket * num_buckets_to_read * rounds * NONCE_SIZE) / (elapsed_time_io2 * 1024 * 1024);
                if (!BENCHMARK)
                    printf("[%.2f] Shuffle %.2f%%: %.2f MB/s\n", omp_get_wtime() - start_time, (i + 1) * 100.0 / num_buckets, throughput_io2);
            }
            // end of for loop
            start_time_io = omp_get_wtime();

            // Flush and close the file
            if (writeData)
            {
                if (fflush(fd) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd);
                    return EXIT_FAILURE;
                }

                if (fsync(fileno(fd)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd);
                    return EXIT_FAILURE;
                }
                fclose(fd);
            }

            if (writeDataFinal)
            {
                if (fflush(fd_dest) != 0)
                {
                    perror("Failed to flush buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }

                if (fsync(fileno(fd_dest)) != 0)
                {
                    perror("Failed to fsync buffer");
                    fclose(fd_dest);
                    return EXIT_FAILURE;
                }

                fclose(fd_dest);

                remove_file(FILENAME);
            }

            free(buffer);
        }
        else if (writeDataFinal && rounds == 1)
        {
            // Call the rename_file function
            if (move_file_overwrite(FILENAME, FILENAME_FINAL) == 0)
            {
                if (!BENCHMARK)
                    printf("File renamed/moved successfully from '%s' to '%s'.\n", FILENAME, FILENAME_FINAL);
            }
            else
            {
                printf("Error in moving file '%s' to '%s'.\n", FILENAME, FILENAME_FINAL);
                return EXIT_FAILURE;
                // Error message already printed by rename_file via perror()
                // Additional handling can be done here if necessary
                // return 1;
            }
        }

// will need to check on MacOS with a spinning hdd if we need to call sync() to flush all filesystems
#ifdef __linux__
        if (DEBUG)
            printf("Final flush in progress...\n");
        int fd2 = open(FILENAME_FINAL, O_RDWR);
        if (fd2 == -1)
        {
            printf("Error opening file %s (#6)\n", FILENAME_FINAL);

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
#endif

        end_time_io = omp_get_wtime();
        elapsed_time_io = end_time_io - start_time_io;
        elapsed_time_io_total += elapsed_time_io;

        if (writeDataTable2 && rounds > 1)
        {
            printf("writing table 2 not implemented with rounds > 1, exiting...\n");
            return -1;
        }
        else if (writeDataTable2 && rounds == 1)
        {
            printf("generating table 2...\n");
            if (generate_table2(FILENAME_FINAL, FILENAME_TABLE2, num_threads, num_threads_io) == 0)
            {
                printf("success in generate_table2\n");
            }
            else
            {
                printf("error in generate_table2\n");
            }
        }

        // End total time measurement
        double end_time = omp_get_wtime();
        double elapsed_time = end_time - start_time;

        // Calculate total throughput
        double total_throughput = (num_iterations / elapsed_time) / 1e6;
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

    //     if (TABLE2)
    //     {
    //         if (!BENCHMARK)
    //             printf("TABLE2 phase has started...\n");

    //         // read data
    //         int fd = open(FILENAME_FINAL, O_RDONLY);
    //         if (fd < 0)
    //         {
    //             perror("Error opening file");
    //             return EXIT_FAILURE;
    //         }

    //         size_t num_buckets_to_read = ceil((MEMORY_SIZE_bytes / (num_records_in_bucket * rounds * NONCE_SIZE)) / 2);
    //         size_t records_per_batch = num_records_in_bucket * num_buckets_to_read;
    //         size_t buffer_size = records_per_batch * rounds; // total number of records in all rounds

    //         MemoRecord *buffer = malloc(buffer_size * sizeof(MemoRecord));
    //         if (buffer == NULL)
    //         {
    //             fprintf(stderr, "Error allocating memory for buffer.\n");
    //             exit(EXIT_FAILURE);
    //         }

    //         if (num_threads_io > 0)
    //         {
    //             omp_set_num_threads(num_threads_io);
    //         }

    //         double elapsed_time_io = 0.0;
    //         double elapsed_time_sort = 0.0;

    //         int read_error = 0;

    //         for (unsigned long long r = 0; r < rounds; r++)
    //         {
    // #pragma omp parallel shared(read_error, buffer)
    //             {
    //                 double local_elapsed_io = 0.0;
    //                 double local_elapsed_sort = 0.0;

    // #pragma omp for schedule(static)
    //                 for (unsigned long long i = 0; i < num_buckets_to_read; i++)
    //                 {
    //                     if (read_error)
    //                         continue;

    //                     off_t offset_src = (r * num_buckets_to_read + i) * num_records_in_bucket * sizeof(MemoRecord);
    //                     size_t buffer_index = i * num_records_in_bucket;

    //                     double start_time_io = omp_get_wtime();
    //                     ssize_t bytes_read = pread(fd, buffer + buffer_index, num_records_in_bucket * sizeof(MemoRecord), offset_src);
    //                     double end_time_io = omp_get_wtime();
    //                     local_elapsed_io += end_time_io - start_time_io;

    //                     if ((long unsigned int)bytes_read != num_records_in_bucket * sizeof(MemoRecord))
    //                     {
    // #pragma omp critical
    //                         {
    //                             fprintf(stderr, "pread failed for bucket %llu (read %zd bytes)\n", i, bytes_read);
    //                             read_error = 1; // Set read_error to indicate failure
    //                         }
    // #pragma omp cancel for
    //                     }

    // #pragma omp cancellation point for

    //                     double start_time_sort = omp_get_wtime();
    //                     // sort data within each bucket
    //                     qsort(buffer + buffer_index, num_records_in_bucket, sizeof(MemoRecord), compare_by_hash);
    //                     double end_time_sort = omp_get_wtime();
    //                     local_elapsed_sort += end_time_sort - start_time_sort;
    //                 }
    // #pragma omp atomic
    //                 elapsed_time_io += local_elapsed_io;
    // #pragma omp atomic
    //                 elapsed_time_sort += local_elapsed_sort;
    //             }

    //             if (read_error)
    //             {
    //                 free(buffer);
    //                 close(fd);
    //                 return EXIT_FAILURE;
    //             }
    //         }

    //         if (!BENCHMARK)
    //         {
    //             printf("[TABLE2]Total I/O Time: %.6f seconds\n", elapsed_time_io);
    //             printf("[TABLE2]Total Sort Time: %.6f seconds\n", elapsed_time_sort);
    //         }

    //         close(fd);
    //         free(buffer);

    //         // write matched records into new file
    //     }

    omp_set_num_threads(num_threads);

    if (SEARCH && !SEARCH_BATCH)
    {
        search_memo_records(FILENAME_FINAL, SEARCH_STRING);
    }

    if (SEARCH_BATCH)
    {
        search_memo_records_batch(FILENAME_FINAL, BATCH_SIZE, PREFIX_SEARCH_SIZE);
    }

    // Call the function to count zero-value MemoRecords
    // printf("verifying efficiency of final stored file...\n");
    // count_zero_memo_records(FILENAME_FINAL);

    // Call the function to process MemoRecords
    if (VERIFY)
    {
        if (!BENCHMARK)
            printf("verifying sorted order by bucketIndex of final stored file...\n");
        process_memo_records(FILENAME_FINAL, MEMORY_SIZE_bytes / sizeof(MemoRecord));
    }

    if (DEBUG)
        printf("SUCCESS!\n");
    return 0;
}