#include "utils.h"
#include "vaultx.h"

// Function to display usage information
void print_usage(char* prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -a, --approach [xtask|task|for|tbb]   Select parallelization approach (default: for)\n");
    printf("  -t, --threads NUM                     Number of threads to use (default: number of available cores)\n");
    printf("  -i, --threads_io NUM                  Number of I/O threads (default: number of available cores)\n");
    printf("  -K, --exponent NUM                    Exponent K to compute 2^K number of records (default: 4)\n");
    printf("  -m, --size of merge batch NUM         Memory size per merge batch in MB (default: 256)\n");
    printf("  -r, --memory limit in MB              Memory size in MB (default: 16384)\n");
    printf("  -b, --batch-size NUM                  Batch size (default: 1024)\n");
    printf("  -S, --lookup-count NUM                Perofrm lookup for NUM hashes in small plots and merged plot\n");
    printf("  -x, --benchmark                       Enable benchmark mode (default: false)\n");
    printf("  -h, --help                            Display this help message\n");
    printf("  -n, --total_files NUM                 Number of K fiels to generate\n");
    printf("  -M, --merge                           Merge k files into a big file\n");
    printf("  -T, --destination (To)                Merge File destination\n");
    printf("  -F, --source (From)                   Small files source\n");
    printf("\nExample:\n");
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo\n", prog_name);
    printf("  %s -a for -t 8 -i 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -x true (Only prints time)\n", prog_name);
    printf("  %s -a for -t 8 -K 25 -m 1024 -f vaultx25_tmp.memo -g vaultx25.memo -2 true\n", prog_name);
}

unsigned char* getRandomHash(size_t num_bytes) {
    FILE* fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    unsigned char* buffer = malloc(num_bytes);
    if (!buffer) {
        perror("malloc");
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buffer, 1, num_bytes, fp);
    if (read != num_bytes) {
        perror("fread");
        free(buffer);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return buffer;
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

void delete_contents(const char* folder_path) {
    DIR* dir = opendir(folder_path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent* entry;
    char path[1024];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively delete subdirectory
                delete_contents(path);
                if (rmdir(path) != 0)
                    perror("rmdir failed");
            } else {
                if (remove(path) != 0)
                    perror("remove failed");
            }
        }
    }

    closedir(dir);
}

void ensure_folder_exists(const char* path) {
    struct stat st = { 0 };

    if (stat(path, &st) == -1) {
        // Folder doesn't exist, try to create it
        if (mkdir(path, 0777) != 0) {
            perror("mkdir failed");
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s exists but is not a directory\n", path);
    }
}
