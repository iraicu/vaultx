# VaultX

A multi-threaded proof-of-space engine that generates, stores, and searches large
hash-indexed datasets on commodity and server hardware.

VaultX computes BLAKE3 hashes over a nonce space, sorts them into buckets on disk, and
builds a compact two-table structure that supports fast record lookup. It is written in
C (with an optional C++/TBB build), parallelised with OpenMP, and has been benchmarked
from a 4-core Raspberry Pi 5 to a 256-thread eight-socket server.

**Paper.** Bondarenko, V., Diaz, R., Nguyen, L., & Raicu, I.
*Improving the Performance of Proof-of-Space in Blockchain Systems.*
SC24 (International Conference for High Performance Computing, Networking, Storage,
and Analysis), 2024.

---

## Build

Requires GCC with OpenMP. The default `Makefile` targets `gcc-13` / `g++-14`; override
`CC` and `CCP` if your toolchain differs.

```sh
make vaultx_x86_c      # x86, C build (gcc + OpenMP)
make vaultx_arm_c      # ARM (Raspberry Pi, Orange Pi)
make vaultx_x86        # x86, C++ build with Intel TBB
make vaultx_mac        # macOS via Homebrew blake3 + tbb
```

Record layout is fixed at compile time:

```sh
make vaultx_x86_c NONCE_SIZE=5 RECORD_SIZE=8    # defaults
```

## Run

```
Usage: ./vaultx [OPTIONS]

  -a [xtask|task|for|tbb]   parallelisation approach (default: for)
  -t NUM                    threads (default: available cores)
  -K NUM                    generate 2^K records (default: 4)
  -m NUM                    in-memory buffer, MB (default: 1)
  -f NAME                   temporary file, raw hashes
  -g NAME                   temporary file, table 1
  -j NAME                   output file, table 2
  -b NUM                    batch size (default: 1024)
  -h, --help                full option list
```

Generate a 2^26-record vault using 16 threads and a 1 GB buffer:

```sh
./vaultx -t 16 -K 26 -m 1024 -f raw.tmp -g table1.tmp -j k26.vault
```

`-K` is the single knob that sets problem size: each increment doubles the record count
and the output file. Memory (`-m`) bounds the in-memory working set independently of `K`,
which is what allows a 4 GB Raspberry Pi to build vaults far larger than its RAM.

## Reproducing the benchmarks

`run-vault.sh` sweeps K up to the maximum each machine can hold and appends one CSV row
per run; `run-vault-search.sh` does the same for lookups. Both select their configuration
by hostname:

| host | max K | memory | threads |
|---|---|---|---|
| `eightsocket` | 35 | 512 GB | 256 |
| `epycbox` | 34 | 256 GB | 128 |
| `orangepi5plus` | 31 | 32 GB | 8 |
| `raspberrypi5` | 28 | 4 GB | 4 |

```sh
./scripts/run-vault.sh          # generation sweep -> data/*.csv
./scripts/run-vault-search.sh   # lookup sweep     -> data/*-lookup.csv
python3 scripts/plot_vaultx.py  # figures          -> figures/*.svg
```

To run on a different machine, add a case to the `hostname` switch in both scripts with
that machine's `max_k`, `memory`, `threads`, build target, and output disks. Storage paths
are per-host and must exist before the sweep starts.

`scripts/drop-all-caches.sh` clears the page cache between runs — required for the I/O
numbers to mean anything, and it needs root.

## Results format

Generation (`data/vaultx-<host>-<medium>.csv`):

```
APPROACH,K,NONCE_SIZE(B),NUM_THREADS,MEMORY_SIZE(MB),FILE_SIZE(GB),BATCH_SIZE,
THROUGHPUT(MH/S),THROUGHPUT(MB/S),HASH_TIME,IO_TIME,SHUFFLE_TIME,OTHER_TIME,
TOTAL_TIME,STORAGE_EFFICIENCY
```

Times are seconds and break total runtime into hashing, I/O, and bucket shuffle, so a
regression can be attributed to a phase rather than guessed at.

Lookup (`data/vaultx-<host>-<medium>-lookup.csv`):

```
FILENAME,NUM_THREADS,FILE_SIZE(GB),NUM_BUCKETS_SEARCH,NUM_RECORDS_IN_BUCKET,
NUM_LOOKUPS,SEARCH_SIZE,FOUND_RECORDS,NOT_FOUND_RECORDS,TOTAL_TIME,TIME_PER_LOOKUP
```

Every figure in the paper is generated from these files by the scripts above; no number is transcribed by hand.

Files are named `vaultx-<host>-<medium>[-lookup|-caching|-extra-run].csv`.`chia-epycbox.csv` holds the Chia plotter baseline measured on the same machine.

## Notes

- `blake3/` is upstream BLAKE3, included so the engine builds without an external
  dependency. It is not part of this project's contribution.
- Build targets and sweep scripts carry hard-coded paths from the machines the SC24
  results were measured on; they are kept as-is so the published runs stay reproducible.

## License

BLAKE3 in `blake3/` is licensed by its authors (CC0 / Apache-2.0). See `blake3/LICENSE`.
