# Compiling

First compile the code: `make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16` or `make vaultx_arm_c NONCE_SIZE=4 RECORD_SIZE=16`,
depending on machine

# Subplot generation
`sudo ./vaultx -a for -K 26 -n 1 -T data-l -F ssd-raid0 -t 128`

- sudo is required to run sync() at the end, to flush everything to disk
- n: number of subplots to generate
- T (to): Destination of merged plot (mount point name)
- F (from): Location to generate subplots (mount point name)
- t for thread counts to use for hash generation


# Merging
`./vaultx -a for -K 26 -n 1 -T data-l -F ssd-raid0 -t 128 -m 1024 -r 16384 -M 1/2`

- M: to specify merge approach (1 for serial, 2 for pipelined. 0 is deprecated)
- m: memory used per batch
- r: total ram limit (needed for pipelined merge approach, as multiple batches may run together)
