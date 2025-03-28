#include "search.h"

// not sure if the search of more than PREFIX_LENGTH works
void search_memo_records(const char *filename, const char *SEARCH_STRING)
{
    uint8_t *SEARCH_UINT8 = hexStringToByteArray(SEARCH_STRING);
    size_t SEARCH_LENGTH = strlen(SEARCH_STRING) / 2;
    off_t bucketIndex = getBucketIndex(SEARCH_UINT8, PREFIX_SIZE);
    // uint8_t *SEARCH_UINT8 = convert_string_to_uint8_array(SEARCH_STRING);
    // num_records_in_bucket
    MemoRecord *buffer = NULL;
    // size_t total_records = 0;
    // size_t zero_nonce_count = 0;

    FILE *file = NULL;
    // uint8_t prev_hash[PREFIX_SIZE] = {0}; // Initialize previous hash prefix to zero
    // uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    // size_t count_condition_met = 0;       // Counter for records meeting the condition
    // size_t count_condition_not_met = 0;
    bool foundRecord = false;
    // MemoRecord fRecord;
    long long fRecord = -1;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
    unsigned long long num_records_in_bucket_search = filesize / num_buckets_search / sizeof(MemoRecord);
    if (!BENCHMARK)
    {
        printf("SEARCH: filename=%s\n", filename);
        printf("SEARCH: filesize=%zu\n", filesize);
        printf("SEARCH: num_buckets=%lluu\n", num_buckets_search);
        printf("SEARCH: num_records_in_bucket=%llu\n", num_records_in_bucket_search);
        printf("SEARCH: SEARCH_STRING=%s\n", SEARCH_STRING);
    }

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(num_records_in_bucket_search * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    fRecord = search_memo_record(file, bucketIndex, SEARCH_UINT8, SEARCH_LENGTH, num_records_in_bucket_search, buffer);
    if (fRecord >= 0)
        foundRecord = true;
    else
        foundRecord = false;

    double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    if (foundRecord == true)
        printf("NONCE found (%llu) for HASH prefix %s\n", fRecord, SEARCH_STRING);
    else
        printf("no NONCE found for HASH prefix %s\n", SEARCH_STRING);
    printf("search time %.2f ms\n", elapsed_time);

    // return NULL;
}

// not sure if the search of more than PREFIX_LENGTH works
void search_memo_records_batch(const char *filename, int num_lookups, int search_size, int num_threads)
{
    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));

    // uint8_t *SEARCH_UINT8 = hexStringToByteArray("000000");
    size_t SEARCH_LENGTH = search_size;
    // uint8_t *SEARCH_UINT8 = convert_string_to_uint8_array(SEARCH_STRING);
    MemoRecord *buffer = NULL;
    // size_t total_records = 0;
    // size_t zero_nonce_count = 0;

    FILE *file = NULL;
    // uint8_t prev_hash[PREFIX_SIZE] = {0}; // Initialize previous hash prefix to zero
    // uint8_t prev_nonce[NONCE_SIZE] = {0}; // Initialize previous nonce to zero
    // size_t count_condition_met = 0;       // Counter for records meeting the condition
    // size_t count_condition_not_met = 0;
    int foundRecords = 0;
    int notFoundRecords = 0;
    // MemoRecord fRecord;
    // long long fRecord = -1;

    long filesize = get_file_size(filename);

    if (filesize != -1)
    {
        if (!BENCHMARK)
            printf("Size of '%s' is %ld bytes.\n", filename, filesize);
    }

    unsigned long long num_buckets_search = 1ULL << (PREFIX_SIZE * 8);
    unsigned long long num_records_in_bucket_search = filesize / num_buckets_search / sizeof(MemoRecord);
    if (!BENCHMARK)
    {
        printf("SEARCH: filename=%s\n", filename);
        printf("SEARCH: filesize=%zu\n", filesize);
        printf("SEARCH: num_buckets=%llu\n", num_buckets_search);
        printf("SEARCH: num_records_in_bucket=%llu\n", num_records_in_bucket_search);
    }

    // Open the file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file %s (#3)\n", filename);

        perror("Error opening file");
        return;
    }

    // Allocate memory for the batch of MemoRecords
    buffer = (MemoRecord *)malloc(num_records_in_bucket_search * sizeof(MemoRecord));
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory.\n");
        fclose(file);
        return;
    }

    // Start walltime measurement
    double start_time = omp_get_wtime();
    // double end_time = omp_get_wtime();

    uint8_t SEARCH_UINT8[search_size];

    for (int i = 0; i < num_lookups; i++)
    {

        for (int i = 0; i < search_size; ++i)
        {
            SEARCH_UINT8[i] = rand() % 256;
        }

        if (search_memo_record(file, getBucketIndex(SEARCH_UINT8, PREFIX_SIZE), SEARCH_UINT8, SEARCH_LENGTH, num_records_in_bucket_search, buffer) >= 0)
            foundRecords++;
        else
            notFoundRecords++;
    }

    double elapsed_time = (omp_get_wtime() - start_time) * 1000.0;

    // Check for reading errors
    if (ferror(file))
    {
        perror("Error reading file");
    }

    // Clean up
    fclose(file);
    free(buffer);

    // Print the total number of times the condition was met
    // if (foundRecord == true)
    //	printf("NONCE found (%zu) for HASH prefix %s\n",fRecord,SEARCH_STRING);
    // else
    //	printf("no NONCE found for HASH prefix %s\n",SEARCH_STRING);
    if (!BENCHMARK)
        printf("searched for %d lookups of %d bytes long, found %d, not found %d in %.2f seconds, %.4f ms per lookup\n", num_lookups, search_size, foundRecords, notFoundRecords, elapsed_time / 1000.0, elapsed_time / num_lookups);
    else
        printf("%s %d %zu %llu %llu %d %d %d %d %.2f %.2f\n", filename, num_threads, filesize, num_buckets_search, num_records_in_bucket_search, num_lookups, search_size, foundRecords, notFoundRecords, elapsed_time / 1000.0, elapsed_time / num_lookups);
    // return NULL;
}