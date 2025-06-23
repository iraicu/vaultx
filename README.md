# VAULTX

**VAULTX** is a Proof-of-Space (PoSp) consensus protocol optimized for performance, security, and efficient memory usage. 

## What Are Vaults? 
Vaults are files stored on users' machines and later used for fast leader election. Each vault is organized into **safe deposit boxes**, where each box contains **valuables**—hashes with potential to win block validation. This structure enables fast, constant-time lookups when searching within a vault.


## Features 

- Efficient vault creation with minimal memory footprint
- Constant-time lookup using bucketed storage (safe deposit boxes)
- Architecture-aware compilation targets
- The boxes allow for constant time lookup when searching through the vault


## Libraries Used

- [`BLAKE3`](https://github.com/BLAKE3-team/BLAKE3) — for fast cryptographic hash functions


## Installation

1. **Clone the repository** 

To use an SSH connection, first add your public SSH key to GitHub.

```bash
  git clone git@github.com:iraicu/vaultx.git
```

2. **Compile the program**

```bash
  make <program_name> NONCE_SIZE=<nonce_size> RECORD_SIZE=16
```

`program_name` options:
  - `vaultx_x86_c` — for x86 architecture
  - `vaultx_arm_c` — for ARM architecture
  - `vaultx_mac_c` — for macOS systems

`nonce_size=4` if `25<=k<=32`
`nonce_size=5` if `33<=k<=40`

## Running

To see available options: 

```
  ./vaultx -h 
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