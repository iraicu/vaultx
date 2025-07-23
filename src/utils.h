#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Usage info
void print_usage(char* prog_name);

// Random bytes generator
unsigned char* getRandomHash(size_t num_bytes);

// Compute bucket index from hash prefix
off_t getBucketIndex(const uint8_t* hash);

// Convert byte array (big endian) to unsigned long long
unsigned long long byteArrayToLongLong(const uint8_t* byteArray, size_t length);

// Check if nonce is non-zero
bool is_nonce_nonzero(const uint8_t* nonce, size_t nonce_size);

// Convert hex string to byte array (caller must free returned pointer)
uint8_t* hexStringToByteArray(const char* hexString);

// Find largest power of two less than the given number
uint64_t largest_power_of_two_less_than(uint64_t number);

// Convert byte array to hex string (caller must free returned pointer)
char* byteArrayToHexString(const unsigned char* bytes, size_t len);

// Convert number to fixed-width hex string (caller must free returned pointer)
char* num_to_hex(unsigned long long num, size_t total_bytes);

// Print bits of byte array
void print_bits(const uint8_t* arr, size_t len);

// Print number in binary (little endian)
void print_number_binary_bytes(uint64_t number, size_t size);

// Recursively delete folder contents
void delete_contents(const char* folder_path);

// Ensure folder exists, create if not
void ensure_folder_exists(const char* path);

#endif // UTILS_H