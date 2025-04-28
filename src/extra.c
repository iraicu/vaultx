#include "globals.h"

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