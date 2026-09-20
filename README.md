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
