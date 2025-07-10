#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

#include "globals.h"
#include "vaultx.h"

void generate_plot_id(uint8_t* plot_id_out);
void derive_key(int k, uint8_t* plot_id, uint8_t* key_out);

// Generate Blake3 hash with one nonce
void generateBlake3(uint8_t* nonce, uint8_t* key, uint8_t* hash);
// Function to generate Blake3 hash with two nonces
void generateBlake3Pair(uint8_t* nonce1, uint8_t* nonce2, uint8_t* key, uint8_t* hash);