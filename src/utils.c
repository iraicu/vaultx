#include "utils.h"
#include "vaultx.h"

// Function to display usage information
void print_usage(char *prog_name) {
  printf("Usage: %s [OPTIONS]\n", prog_name);

  printf("\nCore Options:\n");
  printf("  -k, --exponent NUM                    Exponent K to compute 2^K "
         "records (default: 27, optional for search)\n");
  printf("  -m, --memory NUM                      Memory size in GB (default: "
         "2)\n");
  printf("  -t, --threads NUM                     Number of threads for "
         "generation/search (default: available cores)\n");
  printf("                                        For multi-file search: "
         "splits threads across files and buckets\n");
  printf("  -i, --threads_io NUM                  Number of I/O threads "
         "(default: 1)\n");
  printf("  -a, --approach [xtask|task|for|tbb]   Parallelization approach "
         "(default: for)\n");

  printf("\nFile/Directory Options:\n");
  printf("  -f, --file_table2 PATH                Plot file or directory "
         "(file for search, directory for generation)\n");
  printf("  -g, --file_tmp PATH                   Directory for temporary "
         "Table 1 files\n");
  printf("  -j, --file_tmp_table2 PATH            Directory for temporary "
         "Table 2 files\n");

  printf("\nMerge Mode Options:\n");
  printf("  -P [mode]                             Enable merge workflow (mode: "
         "gen/merge/both or omit for both)\n");
  printf("      -P gen                            Generate N individual plots "
         "only\n");
  printf("      -P merge                          Merge existing plots only\n");
  printf("      -P or -P both                     Generate N plots then merge "
         "them\n");
  printf("  -n, --total_files NUM                 Number of plot files to "
         "generate/merge (required for -P)\n");
  printf("  -F, --source PATH                     Source directory for plot "
         "generation/reading\n");
  printf("  -T, --destination PATH                Destination directory for "
         "merged output file\n");
  printf("  -M, --matching_factor NUM             Matching factor (0.0-1.0, "
         "default: auto)\n");

  printf("\nSearch Options:\n");
  printf("  -s, --search STRING                   Search for specific hash "
         "prefix (hex string)\n");
  printf("  -S NUM                                Perform NUM random lookups "
         "(batch search)\n");
  printf("  -D NUM                                Number of hash bytes to "
         "match (0=full hash, >0=prefix)\n");
  printf("  Note: Search discovers ALL plot files in directory and searches "
         "each one\n");

  printf("\nBatch/Performance Options:\n");
  printf("  -x, --batch_size NUM                  Batch size for operations\n");
  printf("  -W, --write_batch_size_mb NUM         Write batch size in MB\n");
  printf("  -R, --read_batch_size NUM             Read batch size\n");

  printf("\nAdvanced/Debug Options:\n");
  printf("  -w, --memory_write [true|false]       Enable memory write mode "
         "(default: true)\n");
  printf("  -c, --circular_array [true|false]     Use circular array (default: "
         "false)\n");
  printf("  -v, --verify [true|false]             Verify plot after creation "
         "(default: false)\n");
  printf("  -y, --full_buckets [true|false]       Stop when buckets are full "
         "(default: false)\n");
  printf("  -b, --benchmark [true|false]          Enable benchmark output mode "
         "(default: false)\n");
  printf("  -o, --monitor [true|false]            Enable monitoring mode "
         "(default: false)\n");
  printf("  -d, --debug [true|false]              Enable debug output "
         "(default: false)\n");

  printf("\nHelp:\n");
  printf("  -h, --help                            Display this help message\n");

  printf("\nExamples:\n");
  printf("\n  Generate Single Plot (Out-of-Memory Mode):\n");
  printf("    Generate K=27 plot with auto memory:\n");
  printf("      %s -k 27 -g /tmp -j /tmp -f /data\n", prog_name);
  printf("    Generate K=27 with 16GB of RAM:\n");
  printf("      %s -k 27 -m 16 -g /tmp -j /tmp -f /data\n", prog_name);
  printf("    Generate K=28 in-memory with 32GB of RAM:\n");
  printf("      %s -k 28 -m 32 -f /data -t 16\n", prog_name);

  printf("\n  Merge Workflow:\n");
  printf("    Generate 5 plots AND merge them into one (full workflow):\n");
  printf("      %s -P -k 27 -n 5 -F /data -T /output -t 16\n", prog_name);
  printf("    Generate 5 individual plots only (no merge):\n");
  printf("      %s -P gen -k 27 -n 5 -F /data -t 16\n", prog_name);
  printf("    Merge 5 existing plots (no generation):\n");
  printf("      %s -P merge -k 27 -n 5 -F /data -T /output -m 32 -t 32\n",
         prog_name);
  printf("    Creates: k27-{hash}.plot files in /data, merge_27_5.plot in "
         "/output\n");

  printf("\n  Search Operations:\n");
  printf("    Search all plot files in directory:\n");
  printf("      %s -s a1b2c3 -f /data\n", prog_name);
  printf("    Search specific plot file with all 32 threads for Blake3:\n");
  printf("      %s -s a1b2c3 -f /data/k27-abc123def.plot -t 32\n", prog_name);
  printf("    Batch search all files with 1000 lookups each:\n");
  printf("      %s -S 1000 -D 1 -f /data\n", prog_name);
  printf("    Compare search performance with nested parallelism:\n");
  printf("      %s -S 100 -D 3 -f /data -t 32\n", prog_name);
  printf("    Search single large file with maximum parallelism:\n");
  printf("      %s -S 500 -D 3 -f /output/merge_30_5.plot -t 64\n", prog_name);
}

unsigned char *getRandomHash(size_t num_bytes) {
  FILE *fp = fopen("/dev/urandom", "rb");
  if (!fp) {
    perror("fopen");
    return NULL;
  }

  unsigned char *buffer = malloc(num_bytes);
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
off_t getBucketIndex(const uint8_t *hash) {
  off_t index = 0;
  for (size_t i = 0; i < PREFIX_SIZE && i < HASH_SIZE; i++) {
    index = (index << 8) | hash[i];
  }
  return index;
}

// Function to convert bytes to unsigned long long
// FIXME: This has unexpected behavior. It's assuming byte array is in
// big-endian format But works for our use case of comparing distance with
// expected distance
unsigned long long byteArrayToLongLong(const uint8_t *byteArray,
                                       size_t length) {
  unsigned long long result = 0;
  for (size_t i = 0; i < length; ++i) {
    result = (result << 8) | (unsigned long long)byteArray[i];
  }
  return result;
}

// Function to check if a nonce is non-zero
bool is_nonce_nonzero(const uint8_t *nonce, size_t nonce_size) {
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

bool is_record_empty(const MemoTable2Record *record) {
  if (record == NULL) {
    return true;
  }
  return !is_nonce_nonzero(record->nonce1, NONCE_SIZE) &&
         !is_nonce_nonzero(record->nonce2, NONCE_SIZE);
}

uint8_t *hexStringToByteArray(const char *hexString) {
  size_t hexLen = strlen(hexString);
  uint8_t *byteArray = (uint8_t *)malloc(hexLen * sizeof(uint8_t));
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

  // The most significant bit is now set; shift right to get the largest power
  // of 2 less than the original number
  return (number + 1) >> 1;
}

char *byteArrayToHexString(const unsigned char *bytes, size_t len) {
  char *out = malloc(len * 2 + 1); // 2 hex chars per byte + null terminator
  if (out == NULL) {
    return NULL; // Allocation failed
  }

  for (size_t i = 0; i < len; i++) {
    sprintf(out + (i * 2), "%02x", bytes[i]);
  }
  out[len * 2] = '\0'; // Null-terminate
  return out;
}

char *num_to_hex(unsigned long long num, size_t total_bytes) {
  size_t hex_len = total_bytes * 2;

  // Allocate space for hex digits + null terminator
  char *hexstr = malloc(hex_len + 1);
  if (hexstr == NULL)
    return NULL;

  // Format the number with leading zeroes to ensure fixed width
  snprintf(hexstr, hex_len + 1, "%0*llx", (int)hex_len, num);

  return hexstr;
}

void print_bits(const uint8_t *arr, size_t len) {
  for (size_t i = 0; i < len; i++) {
    for (int bit = 7; bit >= 0; bit--) {
      putchar((arr[i] & (1 << bit)) ? '1' : '0');
    }
    printf(" "); // optional space between bytes
  }
  printf("\n");
}

void print_number_binary_bytes(uint64_t number, size_t size) {
  const unsigned char *bytes = (const unsigned char *)&number;

  printf("Binary (little endian, %zu bytes):\n", size);

  for (size_t i = 0; i < size; i++) {
    for (int bit = 7; bit >= 0; bit--) {
      putchar((bytes[i] >> bit) & 1 ? '1' : '0');
    }
    printf(" ");
  }

  printf("\n");
}

void delete_contents(const char *folder_path) {
  DIR *dir = opendir(folder_path);
  if (!dir) {
    perror("opendir failed");
    return;
  }

  struct dirent *entry;
  char path[1024];

  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    path_join(path, sizeof(path), folder_path, entry->d_name);

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

void ensure_folder_exists(const char *path) {
  struct stat st = {0};

  if (stat(path, &st) == -1) {
    // Folder doesn't exist, try to create it
    if (mkdir(path, 0777) != 0) {
      perror("mkdir failed");
    }
  } else if (!S_ISDIR(st.st_mode)) {
    fprintf(stderr, "%s exists but is not a directory\n", path);
  }
}

void path_join(char *dest, size_t dest_size, const char *dir,
               const char *file) {
  size_t dir_len = strlen(dir);
  bool dir_has_slash = (dir_len > 0 && dir[dir_len - 1] == '/');
  bool file_has_slash = (file[0] == '/');

  if (dir_has_slash && file_has_slash) {
    snprintf(dest, dest_size, "%s%s", dir, file + 1);
  } else if (!dir_has_slash && !file_has_slash) {
    snprintf(dest, dest_size, "%s/%s", dir, file);
  } else {
    snprintf(dest, dest_size, "%s%s", dir, file);
  }
}
