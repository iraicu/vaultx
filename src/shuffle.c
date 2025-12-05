#include "shuffle.h"

void shuffle_table2(FILE *fd_src, FILE *fd_dest, size_t buffer_size, size_t records_per_batch, unsigned long long num_buckets_to_read)
{
    // Calculate maximum rounds we can fit in memory
    size_t max_rounds_in_memory = buffer_size / records_per_batch;
    if (max_rounds_in_memory == 0) max_rounds_in_memory = 1;
    if (max_rounds_in_memory > rounds) max_rounds_in_memory = rounds;
    
    // Actual buffer size for the chunk we can process
    size_t chunk_buffer_size = max_rounds_in_memory * records_per_batch;
    
    if (!BENCHMARK) {
        printf("Shuffle chunking: processing %zu rounds at a time (max memory: %zu records)\n", 
               max_rounds_in_memory, chunk_buffer_size);
    }
    
    // Allocate the buffer for the chunk we can handle
    if (DEBUG)
        printf("allocating %zu bytes for buffer\n", chunk_buffer_size * sizeof(MemoTable2Record));
    MemoTable2Record *buffer_table2 = (MemoTable2Record *)malloc(chunk_buffer_size * sizeof(MemoTable2Record));
    if (buffer_table2 == NULL)
    {
        fprintf(stderr, "Error allocating memory for buffer. Requested: %zu MB\n", 
                (chunk_buffer_size * sizeof(MemoTable2Record)) / (1024 * 1024));
        exit(EXIT_FAILURE);
    }

    if (DEBUG)
        printf("allocating %zu bytes for bufferShuffled\n", chunk_buffer_size * sizeof(MemoTable2Record));
    MemoTable2Record *buffer_table2_shuffled = (MemoTable2Record *)malloc(chunk_buffer_size * sizeof(MemoTable2Record));
    if (buffer_table2_shuffled == NULL)
    {
        fprintf(stderr, "Error allocating memory for bufferShuffled. Requested: %zu MB\n", 
                (chunk_buffer_size * sizeof(MemoTable2Record)) / (1024 * 1024));
        free(buffer_table2);
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
    
    double throughput_io = 0.0;

    for (unsigned long long i = 0; i < total_num_buckets; i = i + num_buckets_to_read)
    {
        // Process rounds in chunks that fit in memory
        for (unsigned long long round_chunk_start = 0; round_chunk_start < rounds; round_chunk_start += max_rounds_in_memory)
        {
            unsigned long long rounds_in_this_chunk = max_rounds_in_memory;
            if (round_chunk_start + rounds_in_this_chunk > rounds) {
                rounds_in_this_chunk = rounds - round_chunk_start;
            }
            
            if (DEBUG) {
                printf("Processing bucket group %llu-%llu, round chunk %llu-%llu (%llu rounds)\n", 
                       i, i + num_buckets_to_read - 1, 
                       round_chunk_start, round_chunk_start + rounds_in_this_chunk - 1, 
                       rounds_in_this_chunk);
            }
            
            start_time_io = omp_get_wtime(); 

            // Read data for this chunk of rounds
#pragma omp parallel for schedule(static)
            for (unsigned long long r = 0; r < rounds_in_this_chunk; r++)
            {
                unsigned long long actual_round = round_chunk_start + r;
                
                //  Calculate the source offset
                off_t offset_src = ((actual_round * total_num_buckets + i) * num_records_in_bucket) * sizeof(MemoTable2Record);
                if (DEBUG)
                    printf("read data: offset_src=%jd bytes=%zu\n",
                           (intmax_t)offset_src, records_per_batch * sizeof(MemoTable2Record));

                if (fseeko(fd_src, offset_src, SEEK_SET) < 0)
                {
                    perror("Error seeking in file");
                    fclose(fd_src);
                    exit(EXIT_FAILURE);
                }

                // Store in buffer at position for this chunk
                size_t index = r * records_per_batch;
                if (DEBUG)
                    printf("storing read data at index %zu\n", index);
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
            }

            end_time_io = omp_get_wtime(); 
            elapsed_time_io = end_time_io - start_time_io; 
            elapsed_time_io_total += elapsed_time_io; 

            // Validate unshuffled data for this chunk
            size_t chunk_total_records = rounds_in_this_chunk * records_per_batch;
            for (size_t k = 0; k < chunk_total_records; k++)
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
                        fprintf(stderr, "Warning: Record with one zero nonce at index %zu\n", k);
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
                printf("shuffling %llu buckets with %llu rounds in chunk...\n", num_buckets_to_read, rounds_in_this_chunk);
            
            // Shuffle within this chunk
#pragma omp parallel for schedule(static)
            for (unsigned long long s = 0; s < num_buckets_to_read; s++)
            {
                for (unsigned long long r = 0; r < rounds_in_this_chunk; r++)
                {
                    off_t index_src = ((r * num_buckets_to_read + s) * num_records_in_bucket);
                    off_t index_dest = (s * rounds_in_this_chunk + r) * num_records_in_bucket;

                    memcpy(&buffer_table2_shuffled[index_dest], &buffer_table2[index_src], num_records_in_bucket * sizeof(MemoTable2Record));
                }
            }

            end_time_shuffle = omp_get_wtime(); 
            elapsed_time_shuffle_total += end_time_shuffle - start_time_shuffle;

            // Validate shuffled data for this chunk
            for (size_t k = 0; k < chunk_total_records; k++)
            {
                total_records_processed++;

                if (!is_nonce_nonzero(buffer_table2_shuffled[k].nonce1, NONCE_SIZE) &&
                    !is_nonce_nonzero(buffer_table2_shuffled[k].nonce2, NONCE_SIZE))
                {
                    zero_nonce_count++;
                }
            }

            start_time_io = omp_get_wtime();

            // Calculate destination offset for this chunk
            off_t dest_offset = (i * num_records_in_shuffled_bucket + round_chunk_start * num_buckets_to_read * num_records_in_bucket) * sizeof(MemoTable2Record);
            
            if (fseeko(fd_dest, dest_offset, SEEK_SET) < 0)
            {
                perror("Error seeking in destination file");
                fclose(fd_dest);
                exit(EXIT_FAILURE);
            }

            // Write shuffled chunk to destination
            size_t elementsWritten = fwrite(buffer_table2_shuffled, sizeof(MemoTable2Record), 
                                          num_records_in_bucket * num_buckets_to_read * rounds_in_this_chunk, fd_dest);
            if (elementsWritten != num_records_in_bucket * num_buckets_to_read * rounds_in_this_chunk)
            {
                fprintf(stderr, "Error writing bucket to file; elements written %zu when expected %llu\n",
                        elementsWritten, num_records_in_bucket * num_buckets_to_read * rounds_in_this_chunk);
                fclose(fd_dest);
                exit(EXIT_FAILURE);
            }
            total_bytes_written += elementsWritten * sizeof(MemoTable2Record);

            end_time_io = omp_get_wtime();
            elapsed_time_io = end_time_io - start_time_io;
            elapsed_time_io_total += elapsed_time_io;
            throughput_io = (num_records_in_bucket * num_buckets_to_read * rounds_in_this_chunk * sizeof(MemoTable2Record)) / (elapsed_time_io * 1024 * 1024);

            if (!BENCHMARK)
                printf("[%.2f] Shuffle Table2 %.2f%% (chunk %llu/%llu): %.2f MB/s\n", 
                       omp_get_wtime() - start_time, 
                       (i + 1) * 100.0 / total_num_buckets,
                       round_chunk_start / max_rounds_in_memory + 1,
                       (rounds + max_rounds_in_memory - 1) / max_rounds_in_memory,
                       throughput_io);
        }
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
