#include "crypto.h"

void generate_plot_id(uint8_t* plot_id_out) {
    // NOTE: we could also generate a priv key and derive a pub key from it. But why?
    uint8_t public_key[32];
    // uint8_t pubkey_compressed[33];
    // size_t public_key_len = sizeof(pubkey_compressed);

    // generate 32 random bytes for the private key
    randombytes_buf(public_key, 32);

    // create a secp256k1 context for signing
    // secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    // secp256k1_pubkey pubkey;

    // if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key))
    // {
    //     fprintf(stderr, "Error creating public key\n");
    //     secp256k1_context_destroy(ctx);
    //     return -1;
    // }

    // serialize the public key
    // secp256k1_ec_pubkey_serialize(ctx, pubkey_compressed, &public_key_len, &pubkey, SECP256K1_EC_COMPRESSED);

    // concat public and private keys
    // uint8_t combined[65];
    // memcpy(combined, pubkey_compressed, 33);
    // memcpy(combined + 33, private_key, 32);

    // compute SHA-256 of the combined buffer -> plot_id
    crypto_hash_sha256(plot_id_out, public_key, sizeof(public_key));

    // secp256k1_context_destroy(ctx);

    return 0;
}

// void generate_plot_id(uint8_t* plot_id_out) {

//     uint8_t private_key[32];
//     uint8_t pubkey_compressed[33];
//     size_t public_key_len = sizeof(pubkey_compressed);

//     // generate 32 random bytes for the private key
//     randombytes_buf(private_key, 32);

//     // create a secp256k1 context for signing
//     secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
//     secp256k1_pubkey pubkey;

//     if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key)) {
//         fprintf(stderr, "Error creating public key\n");
//         secp256k1_context_destroy(ctx);
//     }

//     // serialize the public key
//     secp256k1_ec_pubkey_serialize(ctx, pubkey_compressed, &public_key_len, &pubkey, SECP256K1_EC_COMPRESSED);

//     // concat public and private keys
//     uint8_t combined[65];
//     memcpy(combined, pubkey_compressed, 33);
//     memcpy(combined + 33, private_key, 32);

//     // compute SHA-256 of the combined buffer -> plot_id
//     crypto_hash_sha256(plot_id_out, combined, 65);

//     secp256k1_context_destroy(ctx);
// }

// hash plot_id together with k-value to produce the key (unique plots)

void derive_key(int k, uint8_t* plot_id, uint8_t* key_out) {
    uint8_t temp_input[33];

    memcpy(temp_input, plot_id, 32);
    temp_input[32] = k;

    crypto_hash_sha256(key_out, temp_input, sizeof(temp_input));
}

// Function to generate Blake3 hash with one nonce
void generateBlake3(uint8_t* nonce, uint8_t* key, uint8_t* hash) {
    // Ensure that the pointers are valid
    if (nonce == NULL || key == NULL || hash == NULL) {
        fprintf(stderr, "Error: NULL pointer passed to generateBlake3.\n");
        return;
    }

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, key);
    blake3_hasher_update(&hasher, nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, hash, HASH_SIZE);
}

// Function to generate Blake3 hash with two nonces
void generateBlake3Pair(uint8_t* nonce1, uint8_t* nonce2, uint8_t* key, uint8_t* hash) {
    // Ensure that the pointers are valid
    if (nonce1 == NULL || nonce2 == NULL || hash == NULL) {
        fprintf(stderr, "Error: NULL pointer passed to generateBlake3.\n");
        return;
    }

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, key);
    blake3_hasher_update(&hasher, nonce1, NONCE_SIZE);
    blake3_hasher_update(&hasher, nonce2, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, hash, HASH_SIZE);
}