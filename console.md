sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 7.9937 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      157.7361 ms   172.3821 ms     0.1724 ms
  Disk Seek           4878.6509 ms  5331.6393 ms     5.3316 ms
  Disk Read           2071.3100 ms  2263.6335 ms     2.2636 ms
  Record Hashing       206.8840 ms   226.0934 ms     0.2261 ms
  -----------------    ----------   ----------   ----------
  TOTAL               7314.5812 ms  7993.7484 ms     7.9937 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot     85899345920       1000       1000            0          490636               7.3367            7336.66
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0          490636               7.9937            7993.75
TIMING 0.1724 5.3316 2.2636 0.2261 7993.7484 7.9937 7.9937
Peak Memory Usage: 3.23 MB
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 25.8797 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      170.1487 ms   176.2618 ms     0.1763 ms
  Disk Seek              2.2952 ms     2.3777 ms     0.0024 ms
  Disk Read          12524.1774 ms 12974.1438 ms    12.9741 ms
  Record Hashing     12285.5567 ms 12726.9499 ms    12.7269 ms
  -----------------    ----------   ----------   ----------
  TOTAL              24982.1780 ms 25879.7331 ms    25.8797 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot   5497558138880       1000       1000            0        31454091              25.1919           25191.88
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0        31454091              25.8797           25879.73
TIMING 0.1763 0.0024 12.9741 12.7269 25879.7331 25.8797 25.8797
Peak Memory Usage: 218.73 MB
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
[sudo] password for sfatunmbi:
^C^C^C^C
sfatunmbi@epycbox:~/vaultx$ screen -r plotting
There is no screen to be resumed matching plotting.
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
vm.drop_caches = 3
sfatunmbi@epycbox:~/vaultx$
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 26.3766 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      169.4740 ms   175.6417 ms     0.1756 ms
  Disk Seek              2.3025 ms     2.3863 ms     0.0024 ms
  Disk Read          13331.9893 ms 13817.1859 ms    13.8172 ms
  Record Hashing     11946.5775 ms 12381.3542 ms    12.3814 ms
  -----------------    ----------   ----------   ----------
  TOTAL              25450.3432 ms 26376.5681 ms    26.3766 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k39-8b0759f5f602c28a2cdfa64efb8e5d2511b6fc427f345e1b20c76ffc64acfbc0.plot   5497558138880       1000       1000            0        31449398              25.6599           25659.88
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0        31449398              26.3766           26376.57
TIMING 0.1756 0.0024 13.8172 12.3814 26376.5681 26.3766 26.3766
Peak Memory Usage: 216.27 MB
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
vm.drop_caches = 3
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 7.8077 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      150.9950 ms   166.3192 ms     0.1663 ms
  Disk Seek           4804.7897 ms  5292.4180 ms     5.2924 ms
  Disk Read           1927.1490 ms  2122.7315 ms     2.1227 ms
  Record Hashing       205.3571 ms   226.1984 ms     0.2262 ms
  -----------------    ----------   ----------   ----------
  TOTAL               7088.2908 ms  7807.6670 ms     7.8077 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k33-9515443d9e6ec82c348a3ce963907119f34503dc6b7c8423e5843535cec0a534.plot     85899345920       1000       1000            0          489716               7.1099            7109.95
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0          489716               7.8077            7807.67
TIMING 0.1663 5.2924 2.1227 0.2262 7807.6670 7.8077 7.8077
Peak Memory Usage: 2.93 MB
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
vm.drop_caches = 3
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k34-d4e1be161eddad12e7c53cb2c3b9c7104e09a734c6028cdb5900f576d36038b8.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k34-d4e1be161eddad12e7c53cb2c3b9c7104e09a734c6028cdb5900f576d36038b8.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k34-d4e1be161eddad12e7c53cb2c3b9c7104e09a734c6028cdb5900f576d36038b8.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 8.3727 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      151.7061 ms   165.7029 ms     0.1657 ms
  Disk Seek           3328.4676 ms  3635.5618 ms     3.6356 ms
  Disk Read           3793.5527 ms  4143.5571 ms     4.1436 ms
  Record Hashing       391.7158 ms   427.8566 ms     0.4279 ms
  -----------------    ----------   ----------   ----------
  TOTAL               7665.4421 ms  8372.6784 ms     8.3727 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k34-d4e1be161eddad12e7c53cb2c3b9c7104e09a734c6028cdb5900f576d36038b8.plot    171798691840       1000       1000            0          981897               7.6905            7690.53
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0          981897               8.3727            8372.68
TIMING 0.1657 3.6356 4.1436 0.4279 8372.6784 8.3727 8.3727
Peak Memory Usage: 3.26 MB
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
vm.drop_caches = 3
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 8.5016 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      156.5964 ms   171.5670 ms     0.1716 ms
  Disk Seek              2.3392 ms     2.5628 ms     0.0026 ms
  Disk Read           6830.4054 ms  7483.3931 ms     7.4834 ms
  Record Hashing       770.3962 ms   844.0462 ms     0.8440 ms
  -----------------    ----------   ----------   ----------
  TOTAL               7759.7372 ms  8501.5690 ms     8.5016 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot    343597383680       1000       1000            0         1964917               7.7910            7791.02
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0         1964917               8.5016            8501.57
TIMING 0.1716 0.0026 7.4834 0.8440 8501.5690 8.5016 8.5016
Peak Memory Usage: 3.72 MB
sfatunmbi@epycbox:~/vaultx$ sudo sync && sudo sysctl -w vm.drop_caches=3
vm.drop_caches = 3
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 8.3525 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      152.5602 ms   167.4796 ms     0.1675 ms
  Disk Seek              2.1285 ms     2.3366 ms     0.0023 ms
  Disk Read           6683.2290 ms  7336.8058 ms     7.3368 ms
  Record Hashing       770.5519 ms   845.9069 ms     0.8459 ms
  -----------------    ----------   ----------   ----------
  TOTAL               7608.4695 ms  8352.5289 ms     8.3525 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k35-228cd94c4b355eec9b2ee8e3ee229905a3bc1dd57ca3ac17b628076d59787577.plot    343597383680       1000       1000            0         1964522               7.6400            7640.04
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0         1964522               8.3525            8352.53
TIMING 0.1675 0.0023 7.3368 0.8459 8352.5289 8.3525 8.3525
Peak Memory Usage: 3.46 MB
sfatunmbi@epycbox:~/vaultx$ ./vaultx -S 1000 -D 3 -f /data-m/sfatunmbi/plots/k36-27df639e7c979d6cbff9a41c912262e3722108734ad162d3e06ffa7367b526fc.plot
DEBUG: File discovery starting for path: '/data-m/sfatunmbi/plots/k36-27df639e7c979d6cbff9a41c912262e3722108734ad162d3e06ffa7367b526fc.plot'
DEBUG: Path is a regular file
DEBUG: File added: '/data-m/sfatunmbi/plots/k36-27df639e7c979d6cbff9a41c912262e3722108734ad162d3e06ffa7367b526fc.plot'
DEBUG: File discovery complete. SEARCH_FILES_COUNT = 1
SEARCH                      : true

=== New batch search path ===
Files: 1 | Lookups: 1000

--- Timing Breakdown (Total for 1000 lookups on 1 files) ---
  NOTE: 'Avg/Lookup' = wall-clock for one lookup across ALL 1 files.
        'Avg/Lookup/File' = 9.6351 ms (avg_per_lookup / 1 files)
  Component            Cumulative   Wall-Clock   Avg/Lookup
  -----------------    ----------   ----------   ----------
  File Open/Close      151.4846 ms   163.3554 ms     0.1634 ms
  Disk Seek              2.3984 ms     2.5863 ms     0.0026 ms
  Disk Read           7241.7336 ms  7809.2190 ms     7.8092 ms
  Record Hashing      1539.3104 ms  1659.9357 ms     1.6599 ms
  -----------------    ----------   ----------   ----------
  TOTAL               8934.9270 ms  9635.0964 ms     9.6351 ms
------------------------------------------------

Thread config: read(-t)=128 hash(-r)=128 keep_open=0
Filename                                                                            Size (bytes)    Lookups      Found    Not Found     All Matches Avg Time/Lookup (ms)    Total Time (ms)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/data-m/sfatunmbi/plots/k36-27df639e7c979d6cbff9a41c912262e3722108734ad162d3e06ffa7367b526fc.plot    687194767360       1000       1000            0         3929937               8.9781            8978.15
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL (all lookups)                                                                                    1000       1000            0         3929937               9.6351            9635.10
TIMING 0.1634 0.0026 7.8092 1.6599 9635.0964 9.6351 9.6351
Peak Memory Usage: 26.75 MB
sfatunmbi@epycbox:~/vaultx$