# VAULTX

**VAULTX** is a Proof-of-Space (PoSp) consensus protocol optimized for performance, security, and efficient memory usage. 

## What Are Vaults? 
Vaults are files stored on users' machines and later used for fast leader election. Each vault is organized into **safe deposit boxes**, where each box contains **valuables**—hashes with potential to win block validation. This structure enables fast, constant-time lookups when searching within a vault.


## Features 

- Efficient vault creation with minimal memory footprint
- Constant-time lookup using bucketed storage (safe deposit boxes)
- Architecture-aware compilation targets
- Fast in-memory mode for small vaults (k ≤ 32)
- Streaming out-of-memory mode for large vaults
- Automatic vault file discovery and single-lookup or batch search


## Libraries Used

- [`BLAKE3`](https://github.com/BLAKE3-team/BLAKE3) — for fast cryptographic hash functions
- [`OpenMP`](https://www.openmp.org/) — for parallel programming
- [`Sodium`](https://github.com/jedisct1/libsodium) — for secure hashing with SHA-256 and generating random unpredictable data (public key)


## Installation

1. **Clone the repository** 

To use an SSH connection, first add your public SSH key to GitHub.

```bash
  git clone git@github.com:iraicu/vaultx.git
```

When the vaultx repository has been cloned to your system, run the following 
commands to move into the cloned vaultx repository and to check out the git 
submodules associated with it:

```bash
  cd vaultx/
  git submodule update --init --recursive --progress
```

2. **Compile the program**

```bash
  make <program_name> NONCE_SIZE=<nonce_size> RECORD_SIZE=16
```

`program_name` options:
  - `vaultx_x86_c` — for x86 architecture
  - `vaultx_arm_c` — for ARM architecture
  - `vaultx_mac_c` — for macOS systems

`nonce_size=4` if `27<=k<=32`
`nonce_size=5` if `33<=k<=40`

3. **Install libraries**

```bash
  sudo apt install libomp-dev libsodium-dev
```

**Blake3 is already included in the repository**


## Running

To see available options: 

```
  ./vaultx -h 
```

### Command-Line Reference

#### Generation Options

| Flag | Long Name | Description | Required For | Default |
|------|-----------|-------------|--------------|---------|
| `-k` | `--exponent` | Exponent k to compute 2^k records | generation | 27 |
| `-m` | `--memory` | Memory size in MB (out-of-memory mode) | OOM mode | 128 |
| `-g` | `--dir_tmp` | Directory for temporary Table1 file | generation | None |
| `-j` | `--dir_tmp_table2` | Directory for temporary Table2 file | OOM mode | None |
| `-f` | `--dir_table2` | Directory for final vault file | all modes | None |
| `-t` | `--threads` | Number of threads to use | optional | System cores |
| `-x` | `--batch-size` | Batch size for task-based parallelism | optional | 1024 |
| `-a` | `--approach` | Parallelization approach | optional | for |

#### Search Options

| Flag | Long Name | Description | Mode |
|------|-----------|-------------|------|
| `-s` | `--search` | Search for specific hash prefix | Single lookup |
| `-S` | `--prefix_search_size` | Number of bytes for batch search | Batch search |

#### Other Options

| Flag | Long Name | Description |
|------|-----------|-------------|
| `-b` | `--benchmark` | Enable benchmark mode (true/false) |
| `-v` | `--verify` | Enable verification mode (true/false) |
| `-n` | `--monitor` | Enable monitoring (true/false) |
| `-h` | `--help` | Display help message |

### Usage Examples

#### In-Memory Generation (No `-m` Flag, Fast, k ≤ 32)

Generate a vault with k=27 using in-memory mode (all data fits in RAM):

```bash
./vaultx -k 27 -g ./plots/ -f ./plots/
```

**How it works:**
- **No `-m` flag**: Program auto-calculates memory needed
- **Single round**: All data generated and written in one pass
- **Output:** Creates file `k27-<hex_id>.plot` (~1GB for k=27)
- **Time:** ~2-3 minutes on standard hardware
- **Memory:** ~512MB peak usage

#### In-Memory Generation with Benchmark

Generate with performance metrics:

```bash
./vaultx -k 27 -g ./plots/ -f ./plots/ -b true
```

Displays I/O throughput and timing information.


#### Out-of-Memory Generation with `-m` Flag (Multiple Rounds, k > 32 or RAM-Limited)

Generate a vault using streaming mode when memory is limited:

```bash
./vaultx -k 27 -m 128 -g ./plots/ -j ./plots/ -f ./plots/
```

**How it works:**
- **`-m 128`**: Limit memory to 128 MB per round
- **Multiple rounds**: Data split into 4 rounds (512 MB ÷ 128 MB = 4)
- **Temporary file**: `-j` directory stores intermediate Table2 data
- **Final merge**: After all rounds, combines into final vault
- **Output:** Creates file `k27-<hex_id>.plot` (~1GB for k=27)
- **Time:** Longer due to multiple I/O passes (~5-10 minutes for k=27)
- **Memory:** Constant 128 MB throughout

**Why use OOM mode?**
- System has limited RAM (e.g., 4GB laptop)
- Generating very large vaults (k=35 needs 128GB)
- Want finer control over memory usage


#### Single Lookup Search

Search for hash prefix "a1b2c3" in existing vault:

```bash
./vaultx -f ./plots/ -s a1b2c3
```

**Output example:**
```
Size of './plots/k27-ef7ca92d...plot' is 1073741824 bytes
NONCE found (CD842D02, 89852E02) for HASH prefix a1b2c3
search time 1.72 ms
```

**Automatic:**
- Finds vault file automatically if directory contains single `.plot` file
- Extracts plot ID from filename for crypto operations


#### Batch Search (Random Lookups)

Perform 1024 random searches with 3-byte prefixes:

```bash
./vaultx -f ./plots/ -S 3
```

**Flags:**
- `-S 3`: Search size in bytes (3, 4, 6, or 8 supported)

#### Batch Search with Benchmark

```bash
./vaultx -f ./plots/ -S 3 -b true
```

### In-Memory vs Out-of-Memory Modes Explained

The program **automatically chooses** which generation mode to use based on the `-m` flag:

#### Mode Selection Rules

| Condition | Mode | Command Example | Rounds |
|-----------|------|-----------------|--------|
| No `-m` flag | **In-Memory** | `./vaultx -k 27 -g ./ -f ./` | 1 |
| Include `-m` flag | **Out-of-Memory** | `./vaultx -k 27 -m 512 -g ./ -j ./ -f ./` | Multiple |


1. **Vault File Discovery:** When using search mode (`-s` or `-S`), the program automatically discovers the vault file from the directory specified with `-f`
   - **Limitation:** Directory must contain exactly one `.plot` file
   - **Error:** Multiple vaults will cause an error; clean directory before searching

2. **Thread Count:** Default uses all available CPU cores via OpenMP

3. **Memory Selection:** 
   - **No `-m` flag**: Auto-calculated to fit vault (in-memory mode)
   - **With `-m` flag**: Use specified value with multiple rounds (out-of-memory mode)
   - **Minimum**: 128 MB (safety threshold for I/O operations)

4. **Approach:** Default is OpenMP `for` loop parallelization


### Directory Structure

After generation, your directory will contain:

```
./plots/
├── k27-ef7ca92d0a41bcf541d7a7fd49c1180f433d5b6a4b4f6878b60cf46af29c3c10.plot  (1GB)
└── (temporary files in OOM mode, deleted after completion)
```

**Important:** Keep the `.plot` file in the same directory for search operations.


### Troubleshooting

**Error: "No vault file (*.plot) found in directory"**
- Verify generation completed successfully
- Check directory path is correct
- Ensure `.plot` file exists in specified directory

**Error: "Multiple vault files found in directory"**
- Clean directory: `rm <directory>/*.plot`
- Regenerate vault

**Generation stops or is very slow**
- Check available disk space (need ~2-3x vault size free)
- Increase memory with `-m` flag for OOM mode
- Reduce thread count with `-t` if system is overwhelmed
- Monitor I/O with: `iostat -x 1`

**Search returns no results**
- This is expected—most random prefixes won't match
- Vault was generated correctly if search completes without errors
- Search time should be < 10ms for successful lookups

## Benchmarks

Typical performance metrics:

**k=27 (in-memory):** ~2 minutes, 1GB file, ~500 MB/s I/O
**k=28 (in-memory):** ~4 minutes, 2GB file, ~500 MB/s I/O
**Single search:** <10ms on 1GB vault
**Batch search (1024 lookups):** <50ms on 1GB vault

Advanced monitoring with system metrics: 

```bash
./scripts/vaultx_system_monitor_pidstat.py --plot-file ./test.svg --csv-output ./test.csv -- ./vaultx -a for -k 28 -m 512 -W 512 -t 64 -f ./plots/ -g ./plots/ -j ./plots/ -M 1 -x true -n true
```


## Publications

**Bondarenko, Varvara, Renato Diaz, Lan Nguyen, and Ioan Raicu**, _"Improving the Performance of Proof-of-Space in Blockchain Systems"_, **Proceedings of The International Conference for High Performance Computing, Networking, Storage, and Analysis (SC ’24)**, 2024. [📄 PDF](https://sc24.supercomputing.org/proceedings/poster/poster_files/post276s2-file3.pdf) | [🧾 Poster](https://sc24.supercomputing.org/proceedings/poster/poster_files/post276s2-file2.pdf)


## Authors and Contributors

The work is part of research done at [Datasys Lab](https://datasys.cs.iit.edu/) at Illinois Tech that aims to build a decentralized, secure, and scalable blockchain infrastructure, ultimately solving the [blockchain trilemma](https://vitalik.eth.limo/general/2017/12/31/sharding_faq.html#this-sounds-like-theres-some-kind-of-scalability-trilemma-at-play.-what-is-this-trilemma-and-can-we-break-through-it). 

- Arnav Sirigere 
- Renato Diaz
- Varvara Bondarenko
- Lan Nguyen
- Ioan Raicu
- Zack Chaffee
