#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <secp256k1.h>
#include <sodium.h>

#include "globals.h"
#include "vaultx.h"

void generate_plot_id(uint8_t* plot_id_out);
void derive_key(uint8_t* plot_id, uint8_t* key_out);