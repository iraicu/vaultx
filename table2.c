#include "vaultx.h"

// Function to calculate hamming distance between 2 hashes
// Takes in 2 arguments that represent 2 hashes
int hamming_distance(uint8_t *hash1, uint8_t *hash2, size_t hash_size)
{
    int distance = 0;
    for (size_t i = 0; i < hash_size; i++)
    {
        uint8_t xor_result = hash1[i] ^ hash2[i];
        while (xor_result)
        {
            distance += xor_result & 1;
            xor_result >>= 1;
        }
    }
    return distance;
}

void scan_records(MemoRecord *buffer, size_t num_buckets_to_read)
{
// Iterate over buckets in the buffer
#pragma omp parallel for
    for (size_t bucket = 0; bucket < num_buckets_to_read; bucket++)
    {
        for (size_t record = bucket * num_records_in_bucket; record < num_records_in_bucket; record++)
        {
            // hamming_distance(buffer[bucket].hash, SEARCH_UINT8, HASH_SIZE);
        }
    }
}

// Load and process buckets
void process_table1(const char *table1_filename, const char *table2_filename, int num_threads_io, unsigned long long memory_size_bytes, unsigned long long num_buckets, unsigned long long rounds)
{
    FILE *fd = fopen(table1_filename, "rb");
    FILE *fd_dest = fopen(table2_filename, "wb");

    if (!fd || !fd_dest)
    {
        printf("Error opening file %s (#3)\n", table1_filename);
        perror("Error opening file");
        return;
    }

    // rounds = ceil(file_size_bytes / MEMORY_SIZE_bytes);

    // Calculate variables for the number of buckets to read, the number of records to read per batch, and the size of the buffer needed
    unsigned long long num_buckets_to_read = ceil((memory_size_bytes / (num_records_in_bucket * rounds * NONCE_SIZE)) / 2);
    // need to fix this for 5 byte NONCE_SIZE
    if (num_buckets % num_buckets_to_read != 0)
    {
        uint64_t ratio = num_buckets / num_buckets_to_read;
        uint64_t result = largest_power_of_two_less_than(ratio);
        if (DEBUG)
            printf("Largest power of 2 less than %lu is %lu\n", ratio, result);
        num_buckets_to_read = num_buckets / result;
        if (DEBUG)
            printf("will read %llu buckets at one time, %llu bytes\n", num_buckets_to_read, num_records_in_bucket * rounds * NONCE_SIZE * num_buckets_to_read);
    }

    size_t records_per_batch = num_records_in_bucket * num_buckets_to_read;
    size_t buffer_size = records_per_batch * rounds;
    MemoRecord *buffer = (MemoRecord *)malloc(records_per_batch * sizeof(MemoRecord)); // Allocate the buffer

    if (buffer == NULL)
    {
        perror("Error allocating memory for buckets");
        fclose(fd);
        fclose(fd_dest);
        exit(EXIT_FAILURE);
    }

    // Allocate the buffer for the records that go into table2
    if (DEBUG)
        printf("allocating %lu bytes for bufferShuffled\n", buffer_size * sizeof(MemoRecord));
    MemoRecord *buffer_for_table2 = (MemoRecord *)malloc(buffer_size * sizeof(MemoRecord));
    if (buffer_for_table2 == NULL)
    {
        fprintf(stderr, "Error allocating memory for bufferShuffled.\n");
        exit(EXIT_FAILURE);
    }

    // Set the number of threads if specified
    if (num_threads_io > 0)
    {
        omp_set_num_threads(num_threads_io);
    }

    for (unsigned long long i = 0; i < num_buckets; i += num_buckets_to_read)
    {
        double start_time_io2 = omp_get_wtime();
        double elapsed_time_io2 = 0.0;

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

            //  Correct pointer arithmetic
            size_t index = r * records_per_batch;
            if (DEBUG)
                printf("storing read data at index %lu\n", index);
            size_t records_read = fread(&buffer[index],
                                        sizeof(MemoRecord),
                                        records_per_batch,
                                        fd);
            if (records_read != records_per_batch)
            {
                fprintf(stderr, "Error reading file, records read %zu instead of %zu\n",
                        records_read, records_per_batch);
                fclose(fd);
                exit(EXIT_FAILURE);
            }
            else
            {
                if (DEBUG)
                    printf("read %zu records from disk...\n", records_read);
            }

            // Scan through the records in the batch, finding matches
            scan_records(buffer, num_buckets_to_read);

            // off_t offset_dest = i * num_records_in_bucket * NONCE_SIZE * rounds;
            // if (DEBUG)
            //     printf("write data: offset_dest=%lu bytes=%llu\n", offset_dest, num_records_in_bucket * NONCE_SIZE * rounds * num_buckets_to_read);

            // if (fseeko(fd_dest, offset_dest, SEEK_SET) < 0)
            // {
            //     perror("Error seeking in file");
            //     fclose(fd_dest);
            //     exit(EXIT_FAILURE);
            // }
            // needs to make sure its ok, fix things....
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

                memcpy(&buffer_for_table2[index_dest], &buffer[index_src], num_records_in_bucket * sizeof(MemoRecord));
            }
        }
        // end of for loop num_buckets_to_read

        // should write in parallel if possible
        size_t elementsWritten = fwrite(buffer_for_table2, sizeof(MemoRecord), num_records_in_bucket * num_buckets_to_read * rounds, fd_dest);
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
        // elapsed_time_io2_total += elapsed_time_io2;
        double throughput_io2 = (num_records_in_bucket * num_buckets_to_read * rounds * NONCE_SIZE) / (elapsed_time_io2 * 1024 * 1024);
        // if (!BENCHMARK)
        // printf("[%.2f] Shuffle %.2f%%: %.2f MB/s\n", omp_get_wtime() - start_time, (i + 1) * 100.0 / num_buckets, throughput_io2);
    }
    // end of for loop
    // start_time_io = omp_get_wtime();

    // Flush and close the file
    if (writeData)
    {
        if (fflush(fd) != 0)
        {
            perror("Failed to flush buffer");
            fclose(fd);
            exit(EXIT_FAILURE);
        }

        if (fsync(fileno(fd)) != 0)
        {
            perror("Failed to fsync buffer");
            fclose(fd);
            exit(EXIT_FAILURE);
        }
        fclose(fd);
    }

    if (writeDataFinal)
    {
        if (fflush(fd_dest) != 0)
        {
            perror("Failed to flush buffer");
            fclose(fd_dest);
            exit(EXIT_FAILURE);
        }

        if (fsync(fileno(fd_dest)) != 0)
        {
            perror("Failed to fsync buffer");
            fclose(fd_dest);
            exit(EXIT_FAILURE);
        }

        fclose(fd_dest);

        // remove_file(FILENAME);
    }

    free(buffer);
}

void save_to_table2()
{
}

// Function to read buckets from file, find matching hashes, create a hash out of matching hashes and write it to table2 file together with corresponding nonce
// Takes in 5 arguments:
// 1. file: pointer to the file to read from
// 2. bucketIndex: the index of the bucket to search
// 3. SEARCH_UINT8: the search string converted to uint8_t array
// 4. SEARCH_LENGTH: the length of the search string
// 5. num_records_in_bucket_search: the number of records in the bucket
// 6. buffer: the buffer to store the records read from the file
// Returns 0 if the matches are written to table 2, 1 otherwise