#include <vaultx.h>

// Function to calculate hamming distance between 2 hashes
// Takes in 2 arguments that represent 2 hashes
int hamming_distance(int x, int y) {
    int distance = x ^ y; 
    return distance; 
}