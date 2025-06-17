# VAULTX

VAULTX is a Proof of Space consensus protocol optimized for performance, security, and efficient memory usage. 

## Features 

- Generates vaults -- files stored on users' machines used later for fast leader election. 
- Vaults are organized by 'safe deposit boxes', where each box stores valuables. Each valuable has a potential to win in leader election for a user. 
- The boxes allow for constant time lookup when searching through the vault. 


## Installation
1. Clone the repository 
To use ssh connection, first store your public key in github. 
```
  git clone git@github.com:iraicu/vaultx.git
```

2. Compile the program
```
  make <program_name> NONCE_SIZE=<nonce_size> RECORD_SIZE=16
```
program_name = ...
  - x86 architecture: vaultx_x86_c
  - arm architecture: vaultx_arm_c
  - mac: vaultx_mac_c


## Running
```
  ./vaultx -h 
```


## Publications
**Bondarenko, Varvara, Renato Diaz, Lan Nguyen, and Ioan Raicu**, _"Improving the Performance of Proof-of-Space in Blockchain Systems"_, **Proceedings of The International Conference for High Performance Computing, Networking, Storage, and Analysis (SC ’24)**, 2024. [PDF](https://sc24.supercomputing.org/proceedings/poster/poster_files/post276s2-file3.pdf) | [Poster](https://sc24.supercomputing.org/proceedings/poster/poster_files/post276s2-file2.pdf)


## Authors and Contributors
The work is part of research done at Datasys Lab at IIT that aims to build a [decentralized, secure, and scalable blockchain](https://vitalik.eth.limo/general/2017/12/31/sharding_faq.html#this-sounds-like-theres-some-kind-of-scalability-trilemma-at-play.-what-is-this-trilemma-and-can-we-break-through-it). 

- Arnav Sirigere 
- Renato Diaz
- Varvara Bondarenko
- Lan Nguyen
- Ioan Raicu