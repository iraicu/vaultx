#include "search.h"
#include "vaultx.h"

long long search_memo_record(FILE *file, off_t bucketIndex, uint8_t *SEARCH_UINT8, size_t SEARCH_LENGTH, unsigned long long num_records_in_bucket_search, MemoRecord *buffer)
{
    const int HASH_SIZE_SEARCH = 8;
    size_t records_read;
    unsigned long long foundRecord = -1;
    // Define the offset you want to seek to
    long offset = bucketIndex * num_records_in_bucket_search * sizeof(MemoRecord); // For example, seek to byte 1024 from the beginning
    if (DEBUG)
        printf("SEARCH: seek to %zu offset\n", offset);

    // Seek to the specified offset
    if (fseek(file, offset, SEEK_SET) != 0)
    {
        perror("Error seeking in file");
        fclose(file);
        return -1;
    }

    records_read = fread(buffer, sizeof(MemoRecord), num_records_in_bucket_search, file);
    if (records_read > 0)
    {
        int found = 0; // Shared flag to indicate termination

#pragma omp parallel shared(found)
        {
#pragma omp for
            for (size_t i = 0; i < records_read; ++i)
            {
                // Check for cancellation
#pragma omp cancellation point for
                if (!found && is_nonce_nonzero(buffer[i].nonce, NONCE_SIZE))
                {
                    uint8_t hash_output[HASH_SIZE_SEARCH];

                    // Compute Blake3 hash of the nonce
                    blake3_hasher hasher;
                    blake3_hasher_init(&hasher);
                    blake3_hasher_update(&hasher, buffer[i].nonce, NONCE_SIZE);
                    blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE_SEARCH);

                    // print bucket contents
                    if (DEBUG)
                    {
                        printf("bucket[");

                        for (size_t n = 0; n < PREFIX_SIZE; ++n)
                            printf("%02X", SEARCH_UINT8[n]);
                        printf("][%zu] = ", i);

                        for (size_t n = 0; n < NONCE_SIZE; ++n)
                            printf("%02X", buffer[i].nonce[n]);
                        printf(" => ");
                        for (size_t n = 0; n < HASH_SIZE_SEARCH; ++n)
                            printf("%02X", hash_output[n]);
                        printf("\n");
                    }

                    // Compare the first PREFIX_SIZE bytes of the current hash to the previous hash prefix
                    if (memcmp(hash_output, SEARCH_UINT8, SEARCH_LENGTH) == 0)
                    {
                        foundRecord = byteArrayToLongLong(buffer[i].nonce, NONCE_SIZE);

#pragma omp atomic write
                        found = 1;

#pragma omp cancel for
                    }
                    else
                    {
                        //++count_condition_not_met;

                        /*
                                            if (!DEBUG)
                                            {
                                            // Print previous hash and nonce, and current hash and nonce
                                            //printf("Condition not met at record %zu:\n", total_records);
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
    }
    else
    {
        printf("error reading from file..\n");
    }
    return foundRecord;
}

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