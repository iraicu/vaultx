#include "shuffle.h"

void shuffle_table2(FILE *fd_src, FILE *fd_dest, size_t buffer_size, size_t records_per_batch, unsigned long long num_buckets_to_read)
{
    // Allocate the buffer
    if (DEBUG)
        printf("allocating %lu bytes for buffer\n", buffer_size * sizeof(MemoTable2Record));
    MemoTable2Record *buffer_table2 = (MemoTable2Record *)malloc(buffer_size * sizeof(MemoTable2Record));
    if (buffer_table2 == NULL)
    {
        fprintf(stderr, "Error allocating memory for buffer.\n");
        exit(EXIT_FAILURE);
    }

    if (DEBUG)
        printf("allocating %lu bytes for bufferShuffled\n", buffer_size * sizeof(MemoTable2Record));
    MemoTable2Record *buffer_table2_shuffled = (MemoTable2Record *)malloc(buffer_size * sizeof(MemoTable2Record));
    if (buffer_table2_shuffled == NULL)
    {
        fprintf(stderr, "Error allocating memory for bufferShuffled.\n");
        exit(EXIT_FAILURE);
    }

    unsigned long long total_records_unshuffled = 0;
    unsigned long long zero_nonce_count_unshuffled = 0;
    unsigned long long total_records_processed = 0;
    unsigned long long zero_nonce_count = 0;

    double start_time_shuffle = 0.0;
    double end_time_shuffle = 0.0;

    double start_time_io = 0.0;
    double end_time_io = 0.0;
    double elapsed_time_io = 0.0;
    double elapsed_time_io_shuffle = 0.0;

    double throughput_io = 0.0;

    for (unsigned long long i = 0; i < total_num_buckets; i = i + num_buckets_to_read)
    {
        start_time_io = omp_get_wtime(); 

#pragma omp parallel for schedule(static)
        for (unsigned long long r = 0; r < rounds; r++)
        {
            //  Calculate the source offset
            off_t offset_src = ((r * total_num_buckets + i) * num_records_in_bucket) * sizeof(MemoTable2Record);
            if (DEBUG)
                printf("read data: offset_src=%lu bytes=%lu\n",
                       offset_src, records_per_batch * sizeof(MemoTable2Record));

            if (fseeko(fd_src, offset_src, SEEK_SET) < 0)
            {
                perror("Error seeking in file");
                fclose(fd_src);
                exit(EXIT_FAILURE);
            }

            // Correct pointer arithmetic
            size_t index = r * records_per_batch;
            if (DEBUG)
                printf("storing read data at index %lu\n", index);
            size_t records_read = fread(&buffer_table2[index],
                                        sizeof(MemoTable2Record),
                                        records_per_batch,
                                        fd_src);
            if (records_read != records_per_batch)
            {
                fprintf(stderr, "Error reading file, records read %zu instead of %zu\n",
                        records_read, records_per_batch);
                fclose(fd_src);
                exit(EXIT_FAILURE);
            }
            else
            {
                if (DEBUG)
                    printf("read %zu records from disk...\n", records_read);
                total_bytes_read += records_read * sizeof(MemoTable2Record);
            }

            off_t offset_dest = i * num_records_in_shuffled_bucket * sizeof(MemoTable2Record);
            if (DEBUG)
                printf("write data: offset_dest=%lu bytes=%llu\n", offset_dest, num_records_in_shuffled_bucket * sizeof(MemoTable2Record) * num_buckets_to_read);

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

        end_time_io = omp_get_wtime(); 
        elapsed_time_io = end_time_io - start_time_io; 
        elapsed_time_io_total += elapsed_time_io; 

        for (size_t k = 0; k < buffer_size; k++)
        {
            total_records_unshuffled++;

            if (!is_nonce_nonzero(buffer_table2[k].nonce1, NONCE_SIZE) &&
                !is_nonce_nonzero(buffer_table2[k].nonce2, NONCE_SIZE))
            {
                zero_nonce_count_unshuffled++;
            }
            else if (!is_nonce_nonzero(buffer_table2[k].nonce1, NONCE_SIZE) ||
                     !is_nonce_nonzero(buffer_table2[k].nonce2, NONCE_SIZE))
            {
                if (!BENCHMARK)
                {
                    fprintf(stderr, "Warning: Record with one zero nonce at index %lu\n", k);
                    fprintf(stderr, "nonce1: ");
                    for (int b = 0; b < NONCE_SIZE; b++)
                        fprintf(stderr, "%02x", buffer_table2[k].nonce1[b]);
                    fprintf(stderr, "\nnonce2: ");
                    for (int b = 0; b < NONCE_SIZE; b++)
                        fprintf(stderr, "%02x", buffer_table2[k].nonce2[b]);
                    fprintf(stderr, "\n");
                }
            }
        }

        start_time_shuffle = omp_get_wtime();

        if (DEBUG)
            printf("shuffling %llu buckets with %llu bytes each...\n", num_buckets_to_read * rounds, num_records_in_bucket * sizeof(MemoTable2Record));
#pragma omp parallel for schedule(static)
        for (unsigned long long s = 0; s < num_buckets_to_read; s++)
        {
            for (unsigned long long r = 0; r < rounds; r++)
            {
                off_t index_src = ((r * num_buckets_to_read + s) * num_records_in_bucket);
                off_t index_dest = (s * rounds + r) * num_records_in_bucket;

                memcpy(&buffer_table2_shuffled[index_dest], &buffer_table2[index_src], num_records_in_bucket * sizeof(MemoTable2Record));
            }
        }
        // end of for loop num_buckets_to_read

        end_time_shuffle = omp_get_wtime(); 
        elapsed_time_shuffle_total += end_time_shuffle - start_time_shuffle;

        // Validate shuffling - check for all-zero nonces
        for (size_t k = 0; k < buffer_size; k++)
        {
            total_records_processed++;

            if (!is_nonce_nonzero(buffer_table2_shuffled[k].nonce1, NONCE_SIZE) &&
                !is_nonce_nonzero(buffer_table2_shuffled[k].nonce2, NONCE_SIZE))
            {
                zero_nonce_count++;
            }
        }

        start_time_io = omp_get_wtime();

        // should write in parallel if possible
        size_t elementsWritten = fwrite(buffer_table2_shuffled, sizeof(MemoTable2Record), num_records_in_bucket * num_buckets_to_read * rounds, fd_dest);
        if (elementsWritten != num_records_in_bucket * num_buckets_to_read * rounds)
        {
            fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                    elementsWritten, num_records_in_bucket * num_buckets_to_read * rounds);
            fclose(fd_dest);
            exit(EXIT_FAILURE);
        }
        total_bytes_written += elementsWritten * sizeof(MemoTable2Record);

        end_time_shuffle = omp_get_wtime();
        elapsed_time_io_shuffle = end_time_shuffle - start_time_shuffle;
        elapsed_time_shuffle_total += elapsed_time_io_shuffle;
        throughput_io = (num_records_in_bucket * num_buckets_to_read * rounds * sizeof(MemoTable2Record)) / (elapsed_time_io_shuffle * 1024 * 1024);

        // TO DO: Add another Shuffle print statement for 100%
        if (!BENCHMARK)
            printf("[%.2f] Shuffle Table2 %.2f%%: %.2f MB/s\n", omp_get_wtime() - start_time, (i + 1) * 100.0 / total_num_buckets, throughput_io);
    }

    free(buffer_table2);
    free(buffer_table2_shuffled);

    if (!BENCHMARK)
    {
        printf("Zero unshuffled nonces found: %llu out of %llu total records processed.\n",
               zero_nonce_count_unshuffled, total_records_unshuffled);
        printf("Zero nonces found: %llu out of %llu total records processed.\n",
               zero_nonce_count, total_records_processed);
    }
}
