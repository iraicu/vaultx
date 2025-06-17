#include "crypto.h"


void derive_key()
{
    uint8_t key[33];
    memcpy(key, plot_id, 32);
    key[32] = K;

    crypto_hash_sha256(hashed_key, key, sizeof(key));
}

int generate_plot_id()
{
    // NOTE: we could also generate a priv key and derive a pub key from it. But why?
    uint8_t public_key[32];

    // generate 32 random bytes for the private key
    randombytes_buf(public_key, 32);

    // compute SHA-256 of the combined buffer -> plot_id
    crypto_hash_sha256(plot_id, public_key, sizeof(public_key));

    // hash plot_id together with k-value to produce the key (unique plots)
    derive_key();

    return 0;
}