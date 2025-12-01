# VAULTX - Comprehensive Code Explanation

## Table of Contents
1. [Project Overview](#project-overview)
2. [What is VaultX?](#what-is-vaultx)
3. [Core Architecture](#core-architecture)
4. [Data Structures](#data-structures)
5. [Main Program Flow (vaultx.c)](#main-program-flow)
6. [Command-Line Flags Reference](#command-line-flags-reference)
7. [Execution Modes](#execution-modes)
8. [Performance & Optimization](#performance--optimization)
9. [File Formats & Storage](#file-formats--storage)
10. [Dependencies & Libraries](#dependencies--libraries)
11. [Example Usage](#example-usage)

---

## Project Overview

**VAULTX** is a high-performance Proof-of-Space (PoSp) consensus protocol implementation optimized for generating and querying cryptographic vaults used in blockchain systems. The project is part of research at Datasys Lab (Illinois Tech) to address the blockchain trilemma by providing efficient proof-of-space mechanisms.

### Key Authors
- Varvara Bondarenko
- Renato Diaz
- Lan Nguyen
- Ioan Raicu
- Arnav Sirigere

**Publication**: "Improving the Performance of Proof-of-Space in Blockchain Systems" (SC '24)

---

## What is VaultX?

### The Concept
A **vault** is a file stored on a user's machine containing cryptographic hashes organized in a specific structure. These vaults are used for fast leader election in blockchain systems.

### The Structure: Safe Deposit Boxes
Each vault is organized into **safe deposit boxes** (buckets), where each box contains **valuables** (hash entries):
- **Safe Deposit Boxes** = Buckets indexed by hash prefix (bucketed storage)
- **Valuables** = Nonce-hash pairs with potential to win block validation
- **Constant-Time Lookup** = Hash-based bucketing enables O(1) searches

### The Process
1. Generate cryptographic hashes using Blake3 algorithm
2. Organize them into buckets based on hash prefixes
3. Create two table formats:
   - **Table1** = Initial sorted hash records (intermediate)
   - **Table2** = Final formatted records optimized for lookups
4. Store on disk for later use in proof-of-space challenges

---

## Core Architecture

### Main Components

```
vaultx.c (Main Program)
├── Hashing Layer
│   ├── table1.c/h      → Hash generation & Table1 creation
│   ├── table2.c/h      → Table2 generation (matching/pairing nonces)
│   └── crypto.c/h      → Cryptographic operations (using Blake3 & libsodium)
│
├── Data Organization
│   ├── sort.c/h        → Sorting bucket records by hash
│   ├── shuffle.c/h     → Shuffling/reorganizing data
│   └── globals.c/h     → Global variables & shared data structures
│
├── I/O Layer
│   ├── io.c/h          → File I/O operations
│   └── search.c/h      → Searching through generated vaults
│
└── Parallelization
    └── OpenMP          → Multi-threaded execution
    └── TBB             → Intel Threading Building Blocks (C++ mode)
```

### Processing Pipeline
```
Command-Line Args
    ↓
Memory & Round Calculations
    ↓
Bucket Allocation (in-memory structures)
    ↓
[For each round]
    ├→ Hash Generation (nonce → Blake3 hash)
    ├→ Bucket Insertion (by hash prefix)
    ├→ Sorting (by hash value)
    └→ Table2 Creation (nonce pairing)
    ↓
Disk Writing (batched)
    ↓
Cleanup & Report
```

---

## Data Structures

### Core Record Types

#### MemoRecord (Table1)
```c
typedef struct {
    uint8_t nonce[NONCE_SIZE];    // Nonce seed (4-5 bytes typically)
} MemoRecord;
```
- Used during hash generation and Table1 processing
- Contains only the nonce value
- Hash is computed dynamically using Blake3

#### MemoTable2Record (Table2)
```c
typedef struct {
    uint8_t nonce1[NONCE_SIZE];   // First nonce in pair
    uint8_t nonce2[NONCE_SIZE];   // Second nonce in pair
} MemoTable2Record;
```
- Final format stored in vault file
- Contains pairs of nonces that produce "matching" hashes
- Used during lookup operations

#### Bucket Structure (In-Memory)
```c
typedef struct {
    MemoRecord *records;          // Pointer to records array
    size_t count;                 // Number of records stored
    size_t count_waste;           // Records rejected (overflow)
    bool full;                    // Flag: bucket is full
    size_t flush;                 // Number of flushes to disk
} Bucket;
```

#### BucketTable2 Structure
```c
typedef struct {
    MemoTable2Record *records;    // Pointer to Table2 records
    size_t count;                 // Number of records stored
    size_t count_waste;           // Wasted records
    bool full;                    // Bucket full flag
    size_t flush;                 // Flush count
} BucketTable2;
```

### Key Configuration Constants
```c
NONCE_SIZE       // Size of nonce: 4 bytes (K≤32) or 5 bytes (K≥33)
HASH_SIZE        // RECORD_SIZE - NONCE_SIZE
PREFIX_SIZE      // Number of hash prefix bytes for bucketing (usually 3)
RECORD_SIZE      // Total size of hash record (usually 8 bytes)
```

---

## Main Program Flow

### Step-by-Step Execution

#### 1. **Initialization & CLI Parsing**
```c
// Parse command-line arguments
// Validate parameters (K range 24-40, memory ≥64MB)
// Set approach (for/task/xtask/tbb)
// Initialize directories for output files
```

#### 2. **Memory & Round Calculations**
```
Total Records = 2^K
Total Buckets = 2^(PREFIX_SIZE * 8)  // e.g., 2^24 = 16.7M buckets

If data fits in memory (rounds == 1):
  - In-memory mode: generate and process everything at once
  
Else (rounds > 1):
  - Out-of-memory mode: process in multiple streaming rounds
  - Records per round = available memory / record size
  - Need temp files for intermediate data
```

#### 3. **Hash Generation Phase**
```
For each round:
  For each nonce from start_idx to end_idx:
    1. Generate Blake3 hash from nonce
    2. Extract PREFIX_SIZE bytes → bucket index
    3. Insert into in-memory bucket
    4. Track bucket fullness
```

**Parallelization Approaches:**
- `for` (default): OpenMP parallel for loop over batches
- `task`: OpenMP task-based parallelism
- `xtask`: Recursive OpenMP task decomposition
- `tbb`: Intel TBB parallel_for (C++ only)

#### 4. **Table2 Generation Phase**
```
For each bucket:
  1. Sort all records by computed hash value
  2. Scan for "matching" pairs:
     - Find consecutive records with hash distance < threshold
     - Apply matching_factor to determine match probability
     - Create Table2Record with (nonce1, nonce2) pair
  3. Insert into Table2 bucket
  4. Count total matches
```

#### 5. **Disk Writing Phase**
```
Write Table2 records to disk in batches:
  - Batch size = WRITE_BATCH_SIZE_MB
  - Use fwrite() for efficiency
  - Flush & fsync() for data durability
  - Track I/O throughput
```

#### 6. **Out-of-Memory Shuffle** (if rounds > 1)
```
1. Read intermediate Table1 from disk in chunks
2. For each chunk:
   - Sort records by hash
   - Generate Table2 entries
   - Write to temporary Table2 file
3. Shuffle final Table2 to desired layout
4. Clean up intermediate files
```

---

## Command-Line Flags Reference

### Execution Control Flags

#### `-a, --approach [xtask|task|for|tbb]`
**Default:** `for`
**Description:** Parallelization strategy for hash generation
- `for` = OpenMP parallel for (most common, stable)
- `task` = Manual task-based parallelism
- `xtask` = Recursive task decomposition
- `tbb` = Intel TBB (requires C++ compilation)

**Example:**
```bash
./vaultx -a for    # Use OpenMP parallel for
./vaultx -a tbb    # Use Intel TBB (C++ mode only)
```

#### `-t, --threads NUM`
**Default:** Number of available CPU cores
**Description:** Number of worker threads for hash generation
- If set to 0 or omitted, OpenMP automatically uses all available cores
- Useful for limiting parallelism on shared systems
- Must be positive if specified

**Example:**
```bash
./vaultx -t 8      # Use exactly 8 threads
./vaultx -t 128    # Use 128 threads (useful for many-core systems)
```

#### `-i, --threads_io NUM`
**Default:** `1`
**Description:** Number of I/O threads (currently limited usage)
- Intended for parallel I/O operations
- May be used for batched reading/writing
- Note: Implementation may not fully utilize this setting

**Example:**
```bash
./vaultx -i 4      # Request 4 I/O threads
```

### Data Generation Flags

#### `-K, --exponent NUM`
**Range:** 24 to 40
**Default:** 4 (very small, for testing)
**Description:** Exponent for total number of records: 2^K nonces
- K=24 → 16 million records
- K=28 → 268 million records (typical)
- K=34 → ~17 billion records (large)
- K=40 → 1 trillion records (very large)
- Larger K = larger vault = more storage needed

**Compile-Time Constraint:**
- K < 33: Use NONCE_SIZE=4 (compile with `make ... NONCE_SIZE=4`)
- K ≥ 33: Requires NONCE_SIZE=5 (compile with `make ... NONCE_SIZE=5`)

**Example:**
```bash
./vaultx -K 28     # Generate 2^28 = 268M nonces
./vaultx -K 34     # Generate 2^34 = ~17B nonces (requires NONCE_SIZE=5)
```

#### `-m, --memory NUM`
**Minimum:** 64 MB
**Default:** 1 MB
**Description:** Available memory in MB for processing
- Controls how many records fit in RAM at once
- Determines if program uses in-memory (1 round) or streaming (multiple rounds) mode
- Larger memory = fewer disk accesses but higher RAM usage
- Affects batch sizing and round decomposition

**Memory Impact:**
```
If memory > total_data_size:
  - In-memory mode (1 round)
  - Faster, no temp files needed

If memory < total_data_size:
  - Streaming mode (multiple rounds)
  - Requires temp files (-f, -g)
  - Slower due to I/O, but uses less RAM
```

**Example:**
```bash
./vaultx -m 512    # Use 512 MB
./vaultx -m 1024   # Use 1 GB
./vaultx -m 8192   # Use 8 GB (for large vaults)
```

### Batching & I/O Flags

#### `-b, --batch-size NUM`
**Minimum:** 1
**Default:** 1024
**Description:** Internal batch size for task-based processing
- Number of nonces to process per task in task-based parallelism
- Affects granularity of work distribution
- Larger batches = fewer synchronization points but less parallelism
- Smaller batches = more overhead but better load balancing

**Example:**
```bash
./vaultx -b 512    # Process 512 nonces per batch
./vaultx -b 4096   # Process 4096 nonces per batch
```

#### `-W, --write-batch-size NUM`
**Minimum:** 1 MB
**Default:** 1024 MB
**Description:** Disk write batch size in MB
- Controls how many buckets are written in one fwrite() call
- Larger batches = fewer system calls but more buffer memory
- Smaller batches = more I/O calls but lower memory overhead

**Calculation:**
```
buckets_per_write = (WRITE_BATCH_SIZE_MB * 1024 * 1024) / 
                    (num_records_in_bucket * sizeof(MemoTable2Record))
```

**Example:**
```bash
./vaultx -W 256    # Write 256 MB at a time
./vaultx -W 2048   # Write 2 GB at a time (for large memory systems)
```

#### `-R, --read-batch-size NUM`
**Minimum:** 1 MB
**Default:** 1024 MB
**Description:** Disk read batch size in MB (out-of-memory mode)
- Controls how many buckets are read in one fread() call
- Only used when processing multiple rounds from disk
- Must be ≤ number of buckets that fit in memory

**Constraint:**
```
READ_BATCH_SIZE must be ≤ num_diff_pref_buckets_to_read
or program adjusts it automatically with a warning
```

**Example:**
```bash
./vaultx -R 512    # Read 512 MB at a time
```

### Algorithm Flags

#### `-M, --matching-factor NUM`
**Range:** 0.0 < factor ≤ 1.0
**Default:** Varies by K (hardcoded in code)
**Description:** Factor controlling which nonce pairs are considered "matching"
- Lower factor = more stringent matching = fewer matches but higher storage efficiency
- Higher factor = looser matching = more matches but lower efficiency
- Typically optimized via `scripts/match_factor_search.py`
- Affects Table2 compression ratio

**Typical Hardcoded Values (by K):**
```
K=25: 0.11680
K=26: 0.00010
K=27: 0.13639
K=28: 0.33318
K=29: 0.50763
K=30: 0.62341
K=31: 0.73366
K=32: 0.83706
K≥33: 1.0 (unless overridden with -M)
```

**Example:**
```bash
./vaultx -M 0.5    # Use 0.5 matching factor
./vaultx -M 0.1    # Use 0.1 (stricter matching)
```

### Output/Directory Flags

#### `-f, --file_tmp DIR`
**Required for:** Hash generation (HASHGEN)
**Description:** Directory for temporary Table1 file (or final Table2 in in-memory mode)
- For in-memory mode: final Table2 written here
- For out-of-memory mode: unshuffled Table1 written here
- Must be a valid existing directory path

**Output Format:**
```
{DIR}/k{K}-{PLOT_ID}.tmp    [out-of-memory Table1]
{DIR}/k{K}-{PLOT_ID}.plot   [in-memory Table2]
```

**Example:**
```bash
./vaultx -f /tmp/vaults/           # Use /tmp/vaults/ for temp storage
./vaultx -f /mnt/ssd/vault_tmp/    # Use specific SSD mount
```

#### `-g, --file_tmp_table2 DIR`
**Required for:** Out-of-memory mode (rounds > 1)
**Description:** Directory for temporary Table2 file during shuffling
- Only used when data doesn't fit in memory
- Intermediate Table2 before final shuffle to -j location
- Can be same as -f or -j for simplicity

**Example:**
```bash
./vaultx -g /tmp/table2_tmp/   # Temporary Table2 location
```

#### `-j, --file_table2 DIR`
**Required for:** All modes (hash generation and search)
**Description:** Directory for final Table2 output file
- Final vault file location
- Used for all search operations
- Must be writable directory

**Output Format:**
```
{DIR}/k{K}-{PLOT_ID}.plot
```

**Example:**
```bash
./vaultx -j /home/vaults/final/   # Final vault storage
```

### Search Flags

#### `-s, --search STRING`
**Description:** Search for specific nonce prefix in existing vault
- Disables HASHGEN automatically
- Searches Table2 for records matching the prefix
- Requires `-j` flag to specify vault location

**Example:**
```bash
./vaultx -s "ab1234" -j /home/vaults/   # Search for prefix
```

#### `-S, --search-batch NUM`
**Description:** Batch search mode with multiple prefixes
- Allows searching multiple prefixes efficiently
- NUM specifies prefix size for batch searches
- Also disables HASHGEN

**Example:**
```bash
./vaultx -S 64 -j /home/vaults/   # Batch search with 64-byte prefixes
```

### Tuning & Debug Flags

#### `-v, --verify [true|false]`
**Default:** `false`
**Description:** Enable verification mode
- Prints validation information during generation
- Verifies sorted order and storage efficiency
- Adds computational overhead but useful for debugging

**Example:**
```bash
./vaultx -v true    # Enable verification
./vaultx -v false   # Disable verification (default)
```

#### `-x, --benchmark [true|false]`
**Default:** `false`
**Description:** Enable benchmark/quiet mode
- Suppresses normal progress output
- Only prints final timing and metrics
- Useful for performance measurements
- Does not measure vault contents

**Example:**
```bash
./vaultx -x true    # Benchmark mode (minimal output)
```

#### `-w, --memory_write [true|false]`
**Default:** true (usually)
**Description:** Enable/disable writing to in-memory buckets during generation
- If true: records inserted into memory buckets
- If false: records discarded (testing only)

#### `-c, --circular_array [true|false]`
**Default:** false
**Description:** Use circular array for bucket storage
- Alternative memory layout strategy
- May improve cache locality
- Research/optimization flag

#### `-n, --monitor [true|false]`
**Default:** `false`
**Description:** Enable monitoring mode
- Prints PID and stage markers for external monitoring
- Outputs "VAULTX_STAGE_MARKER" messages with timestamps
- Used with monitoring tools like `vaultx_system_monitor_pidstat.py`

**Example:**
```bash
./vaultx -n true -x true    # Monitor + benchmark
# Output includes: VAULTX_PID: 12345
#                  VAULTX_STAGE_MARKER: [time] START Table1Gen
```

#### `-y, --full_buckets [true|false]`
**Default:** `false`
**Description:** Overgenerate hashes to fill all buckets
- If true: generates extra nonces to try filling every bucket completely
- Used for optimization studies

#### `-d, --debug [true|false]`
**Default:** `false`
**Description:** Enable debug output
- Prints detailed bucket contents
- Prints hash calculations
- Very verbose, slows down execution
- Useful for troubleshooting

**Example:**
```bash
./vaultx -d true    # Enable debug output
```

#### `-h, --help`
**Description:** Display usage information and exit

---

## Execution Modes

### Mode 1: In-Memory Generation (Fast, Single Round)

**Condition:** `memory >= total_data_size`

```bash
./vaultx -a for -t 8 -i 8 -K 28 -m 1024 -f /tmp/vaults/ -j /tmp/vaults/
```

**What Happens:**
1. All 2^K nonces fit in available memory
2. Generate hashes, populate buckets in RAM
3. Sort and create Table2 in memory
4. Write final Table2 directly to disk (single round)
5. No temporary files needed

**Advantages:**
- Fast (no intermediate disk I/O)
- Simple (single round)
- Only -f and -j needed (no -g)

**Output Files:**
- `k28-{PLOT_ID}.plot` (final vault)

---

### Mode 2: Out-of-Memory Generation (Streaming, Multiple Rounds)

**Condition:** `memory < total_data_size`

```bash
./vaultx -a for -t 8 -K 28 -m 512 -f /tmp/t1/ -g /tmp/t2_tmp/ -j /tmp/vaults/
```

**What Happens:**
1. Data split into multiple rounds based on available memory
2. Each round:
   - Generate nonces (2^K / rounds each)
   - Populate temporary buckets in memory
   - Write unshuffled Table1 to temp file (-f)
3. After all Table1 written:
   - Read chunks of Table1 from disk
   - Sort and generate Table2
   - Write to temporary Table2 file (-g)
4. Shuffle final Table2 to destination (-j)
5. Clean up intermediate files

**Advantages:**
- Works with limited memory
- Scalable to very large K values

**Disadvantages:**
- Slower (multiple disk I/O passes)
- More complex setup (all 3 directories needed)

**Output Files (Intermediate):**
- `k28-{PLOT_ID}.tmp` (unsorted Table1)
- `k28-{PLOT_ID}.tmp2` (temporary Table2)

**Output Files (Final):**
- `k28-{PLOT_ID}.plot` (shuffled Table2)

---

### Mode 3: Search in Existing Vault

**Condition:** vault file already exists from previous generation

```bash
./vaultx -s "prefix_hex" -j /tmp/vaults/
```

**What Happens:**
1. Load existing Table2 from -j location
2. Search for nonce pairs matching the given prefix
3. Return matching records for proof-of-space challenge

**Requirements:**
- Vault file must exist
- Only -j flag needed (vault location)
- -K can be omitted (inferred from file)

---

### Mode 4: Benchmark Mode

**Condition:** measure performance without full output

```bash
./vaultx -a for -t 128 -K 28 -m 512 -f /tmp/v/ -g /tmp/v/ -j /tmp/v/ -x true -n true
```

**Output (Minimal):**
```
VAULTX_PID: 12345
[timestamp] VAULTX_STAGE_MARKER: START Table1Gen
...
[timestamp] VAULTX_STAGE_MARKER: END Table1Gen
HashGen 100.00%: Total 850.23 MH/s 1024.50 MB/s : I/O 2048.75 MB/s
Total matches found: 125000000 out of 250000000 records
Percentage of matches: 50.00%
```

**Use Cases:**
- Performance testing
- Automated testing pipelines
- Cluster benchmarking

---

## Performance & Optimization

### Key Metrics

#### 1. **Hash Throughput (MH/s - Million Hashes/Second)**
```
MH/s = (num_records_per_round / elapsed_time_hash) / 1e6
```
- Affected by: parallelization approach, thread count, CPU capabilities
- Typical range: 100-1000 MH/s depending on hardware

#### 2. **I/O Throughput (MB/s)**
```
MB/s = (data_written / elapsed_time_io) / (1024 * 1024)
```
- Affected by: batch size, disk speed, write method
- Typical range: 500-3000 MB/s depending on storage

#### 3. **Storage Efficiency**
```
efficiency = (total_matches / total_records) * 100%
```
- Affected by: matching_factor
- Goal: ~100% (ideally one Table2 entry per two Table1 nonces)
- Varies by K value

#### 4. **Bucket Fill Efficiency**
```
bucket_efficiency = (full_buckets / total_num_buckets) * 100%
```
- Indicates how well buckets are utilized
- Higher = better data distribution

### Optimization Tips

#### For Speed:
1. Increase thread count (`-t`) to match CPU cores
2. Use `-a for` (most optimized)
3. Increase batch sizes (`-b`, `-W`, `-R`)
4. Use fast storage (NVMe SSD preferred)

#### For Memory:
1. Reduce `-m` if memory-constrained
2. Accept multiple rounds (slower but works)
3. Monitor with `-n true` to track progress

#### For Accuracy:
1. Enable verification (`-v true`)
2. Use debug mode (`-d true`) for troubleshooting
3. Monitor matching factor with `scripts/match_factor_search.py`

#### For Production:
1. Use benchmark mode (`-x true`) for clean measurements
2. Enable monitoring (`-n true`) for progress tracking
3. Test with smaller K first, then scale up
4. Validate output files after generation

---

## File Formats & Storage

### Naming Convention
```
k{K}-{PLOT_ID}.{extension}

k28-a1b2c3d4e5f6.plot    [final vault]
k28-a1b2c3d4e5f6.tmp     [temporary Table1]
k28-a1b2c3d4e5f6.tmp2    [temporary Table2]
```

Where PLOT_ID is a 32-byte random identifier (hex-encoded).

### File Layout

#### Table2 File Format (.plot)
```
Binary format, sequential:

[Bucket 0]
  [Record 0: nonce1 (NONCE_SIZE) | nonce2 (NONCE_SIZE)]
  [Record 1: nonce1 (NONCE_SIZE) | nonce2 (NONCE_SIZE)]
  ...
  [Record N: nonce1 (NONCE_SIZE) | nonce2 (NONCE_SIZE)]
[Bucket 1]
  ...
[Bucket 2^(PREFIX_SIZE*8)-1]
  ...
```

**Total Size:**
```
File_Size = (2^(PREFIX_SIZE*8)) * (num_records_in_bucket) * 
            sizeof(MemoTable2Record)

For typical case (PREFIX_SIZE=3, 2^28 nonces):
File_Size ≈ 16M buckets * 1M records/bucket * 10 bytes ≈ 160GB
```

#### Storage Efficiency
```
Table2_Size / Table1_Size = matching_factor

If matching_factor = 0.5:
  Table2 is 50% the size of Table1
  Every 2 Table1 records compress to 1 Table2 record
```

---

## Dependencies & Libraries

### Required System Libraries

#### 1. **BLAKE3**
- Purpose: Cryptographic hash function (embedded in project)
- Location: `blake3/` directory
- Features: Fast, parallelizable, deterministic
- Used for: All nonce hashing

#### 2. **OpenMP**
- Purpose: Multi-threaded parallelization
- Package: `libomp-dev` (Debian) or built-in with GCC
- Used for: Parallel for loops, tasks, synchronization

#### 3. **libsodium**
- Purpose: Cryptographic utilities, random number generation
- Package: `libsodium-dev` (Debian) or `libsodium-devel` (Fedora)
- Used for: SHA-256 keying, plot_id generation

#### 4. **libnuma** (Optional, for NUMA systems)
- Purpose: NUMA-aware memory allocation
- Package: `libnuma-dev` (Debian)
- Used for: Optimized memory placement on multi-socket systems

#### 5. **Intel TBB** (Optional, for TBB mode)
- Purpose: Task Parallel Library for -a tbb approach
- Package: `libtbb-dev` (Debian)
- Used for: Alternative to OpenMP parallelization

### Compilation Targets

#### x86 with C
```bash
make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16
```
- Uses GCC (`gcc`)
- Supports OpenMP parallelism
- Optimized assembly for x86-64 (SSE2, SSE4.1, AVX2, AVX-512)

#### x86 with C++
```bash
make vaultx_x86 NONCE_SIZE=5 RECORD_SIZE=16
```
- Uses G++ (`g++-14`)
- Supports TBB parallelism
- Better for large K values

#### ARM
```bash
make vaultx_arm_c NONCE_SIZE=4 RECORD_SIZE=16
```
- For ARM architectures
- NEON support included

#### macOS
```bash
make vaultx_mac_c NONCE_SIZE=4 RECORD_SIZE=16
```
- Homebrew dependencies
- Uses system compiler

---

## Example Usage

### Example 1: Small In-Memory Test
```bash
# Compile
make vaultx_x86_c NONCE_SIZE=4 RECORD_SIZE=16

# Generate small vault (K=24, ~100MB, in-memory)
./vaultx -a for -t 8 -K 24 -m 512 -f ./plots/ -j ./plots/

# Output
# Selected Approach: for
# Number of Threads: 8
# Exponent K: 24
# Table1 File Size (GB): 0.10
# Table2 File Size (GB): 0.20
# ...
# [timestamps with progress markers]
# Total matches found: 5000000 out of 10000000 records
# Percentage of matches: 50.00%
```

### Example 2: Large Out-of-Memory Production Run
```bash
# Compile for large K
make vaultx_x86 NONCE_SIZE=5 RECORD_SIZE=16

# Generate K=34 vault with 4GB memory limit
./vaultx -a for -t 128 -i 16 -K 34 -m 4096 \
  -W 512 -R 256 \
  -f /mnt/ssd1/vaults/ \
  -g /mnt/ssd2/temp_t2/ \
  -j /mnt/nvme/final/ \
  -M 0.72 \
  -x true \
  -n true

# Monitor in separate terminal
python3 scripts/vaultx_system_monitor_pidstat.py --pid 12345
```

### Example 3: Optimize Matching Factor
```bash
# Find best matching factor for K=34
python3 scripts/match_factor_search.py 34 1024

# Output
# --- Final Result ---
# Most accurate match factor found: 0.7235
# Search method: Fast binary search
```

### Example 4: Search in Existing Vault
```bash
# Search for specific prefix in vault
./vaultx -s "00abcd" -j /mnt/nvme/final/

# Output
# [search results with matching nonce pairs]
```

### Example 5: Benchmark Comparison
```bash
# Test different parallelization approaches
for approach in for task tbb; do
  echo "Testing $approach:"
  ./vaultx -a $approach -t 64 -K 28 -m 1024 \
    -f ./plots/ -j ./plots/ -x true
done
```

---

## Troubleshooting & Common Issues

### Build Issues

#### Missing Blake3
```
Error: blake3.h not found
Solution: Blake3 is embedded; ensure blake3/ directory exists
```

#### Missing libsodium
```
Error: sodium.h not found
Solution: sudo apt install libsodium-dev (Debian/Ubuntu)
         sudo dnf install libsodium-devel (Fedora)
```

#### TBB not found (-a tbb fails)
```
Error: tbb/parallel_for.h not found
Solution: 
  1. Install: sudo apt install libtbb-dev
  2. Use C++ target: make vaultx_x86
  3. Avoid -a tbb if not available, use -a for instead
```

### Runtime Issues

#### "K >= 33 requires NONCE_SIZE to be 5"
```
Solution: Recompile with NONCE_SIZE=5:
  make vaultx_x86 NONCE_SIZE=5 RECORD_SIZE=16
```

#### Out of memory errors
```
Solution:
  1. Reduce -m (memory)
  2. Program will use multiple rounds (slower but works)
  3. Or increase system RAM
```

#### Slow I/O
```
Solutions:
  1. Increase -W (write batch size)
  2. Use faster storage (NVMe > SSD > HDD)
  3. Check disk usage: monitor with 'iostat'
  4. Use parallel SSDs if available
```

#### Verification failures (-v true)
```
Solutions:
  1. Check hashing consistency
  2. Verify sorting correctness
  3. Use -d true for detailed debug output
  4. Contact developers if persistent
```

---

## Performance Benchmarks (Reference)

### Typical Throughput (Modern Hardware)
```
Hash Generation:     800-1200 MH/s (multi-threaded)
I/O Throughput:      1000-3000 MB/s (NVMe SSD)
Table2 Efficiency:   ~50% (half the records of Table1)

Generation Time:
  K=24 (16M nonces):      ~30 seconds (in-memory)
  K=28 (268M nonces):     ~5 minutes (in-memory, 8 threads)
  K=34 (17B nonces):      ~2-4 hours (streaming, 128 threads)
```

### Memory Usage
```
K=24 + 512MB memory:  Full in-memory
K=28 + 1GB memory:    Full in-memory
K=34 + 4GB memory:    Multiple rounds needed
K=40 + 8GB memory:    Many rounds needed
```

---

## References & Further Reading

- **VAULTX Publication**: SC '24 Conference Proceedings
- **Datasys Lab**: https://datasys.cs.iit.edu/
- **Blake3**: https://github.com/BLAKE3-team/BLAKE3
- **libsodium**: https://doc.libsodium.org/
- **OpenMP**: https://www.openmp.org/
- **Intel TBB**: https://github.com/oneapi-src/oneTBB

---

## Appendix: Quick Reference Commands

```bash
# Display help
./vaultx -h

# Small test (K=24, ~100MB)
./vaultx -K 24 -m 512 -f ./plots/ -j ./plots/

# Production small (K=28)
./vaultx -a for -t 128 -K 28 -m 2048 -W 512 \
  -f /ssd1/ -j /ssd2/

# Production large (K=34)
./vaultx -a for -t 128 -K 34 -m 4096 \
  -f /ssd1/ -g /ssd2/ -j /nvme/

# Benchmark mode
./vaultx -K 28 -m 1024 -f ./plots/ -j ./plots/ -x true

# With monitoring
./vaultx -K 28 -m 1024 -f ./plots/ -j ./plots/ -n true -x true

# Find optimal matching factor
python3 scripts/match_factor_search.py 34 1024

# Search existing vault
./vaultx -s "00abcd" -j /nvme/final/
```

---

**Generated:** November 2025
**Based On:** vaultx.c and supporting source files
**Project Repository:** https://github.com/iraicu/vaultx
