#include "../blake3/blake3.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <omp.h>
#include <stdbool.h>
#include <time.h>

#define NONCE_SIZE 4
#define HASH_SIZE 12

void generate_random_key(unsigned char *key, size_t length)
{
    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    for (size_t i = 0; i < length; i++)
    {
        key[i] = rand() % 256;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "Usage: %s <K> <table1|table2>\n", argv[0]);
        fprintf(stderr, "key is optional, if provided, it will update hasher with key.\n");
        return 1;
    }

    int K = atoi(argv[1]);
    unsigned long long num_iterations = 1ULL << K;

    bool table2 = false;
    if (strcasecmp(argv[2], "table2") == 0)
    {
        table2 = true;
    }
    else if (strcasecmp(argv[2], "table1") == 0)
    {
        table2 = false;
    }
    else
    {
        fprintf(stderr, "Invalid argument. Use table2 or table1.\n");
        return 1;
    }

    bool add_key = false;
    if (argc > 3 && strcmp(argv[3], "key") == 0)
    {
        add_key = true;
    }

    uint8_t key[BLAKE3_KEY_LEN];
    uint8_t hash_output[HASH_SIZE];
    uint8_t hash_output2[HASH_SIZE];
    uint8_t nonce1[NONCE_SIZE];
    uint8_t nonce2[NONCE_SIZE];

    generate_random_key((unsigned char *)key, BLAKE3_KEY_LEN);

    double start_time_keyed = omp_get_wtime();

    // Keyed BLAKE3 hash
    for (unsigned long long i = 0; i < num_iterations; i++)
    {
        memcpy(nonce1, &i, NONCE_SIZE);

        blake3_hasher hasher;
        blake3_hasher_init_keyed(&hasher, key);
        blake3_hasher_update(&hasher, nonce1, NONCE_SIZE);
        if (table2)
        {
            unsigned long long j = i + 1; // Example for table2, using nonce + 1 as a second nonce
            memcpy(nonce2, &j, NONCE_SIZE);
            blake3_hasher_update(&hasher, nonce2, NONCE_SIZE);
        }
        blake3_hasher_finalize(&hasher, hash_output, HASH_SIZE);
    }
    double elapsed_time_keyed = omp_get_wtime() - start_time_keyed;

    // Unkeyed BLAKE3 hash
    double start_time_unkeyed = omp_get_wtime();
    for (unsigned long long i = 0; i < num_iterations; i++)
    {
        memcpy(nonce1, &i, NONCE_SIZE);

        blake3_hasher hasher2;
        blake3_hasher_init(&hasher2);
        if (add_key)
        {
            blake3_hasher_update(&hasher2, key, BLAKE3_KEY_LEN);
        }
        blake3_hasher_update(&hasher2, nonce1, NONCE_SIZE);
        if (table2)
        {
            unsigned long long j = i + 1;
            memcpy(nonce2, &j, NONCE_SIZE);
            blake3_hasher_update(&hasher2, nonce2, NONCE_SIZE);
        }
        blake3_hasher_finalize(&hasher2, hash_output2, HASH_SIZE);
    }
    double elapsed_time_unkeyed = omp_get_wtime() - start_time_unkeyed;

    printf("Keyed BLAKE3 hash time: %.6f seconds\n", elapsed_time_keyed);
    printf("Unkeyed BLAKE3 hash time: %.6f seconds\n", elapsed_time_unkeyed);
}