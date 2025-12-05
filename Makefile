NAME=blake3/blake3
CC=gcc-13
CCP=g++-14
#torus
#XCC=/ssd-raid0/shared/xgcc/bin/xgcc
#s8
XCC=/home/wwang/xgcc/bin/xgcc

CFLAGS=-g -O3 -DBLAKE3_USE_NEON=0 -Wall -Wextra -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -fvisibility=hidden 

#CFLAGS=-O3 -Wall -Wextra -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -fvisibility=hidden
LDFLAGS=-lm -lpthread -pie -Wl,-z,relro,-z,now -lsodium
# -lsecp256k1
TARGETS=
ASM_TARGETS=
EXTRAFLAGS=-Wa,--noexecstack

# You can set values with a default, but allow it to be overridden
NONCE_SIZE ?= 5
RECORD_SIZE ?= 8

ifdef BLAKE3_NO_SSE2
EXTRAFLAGS += -DBLAKE3_NO_SSE2
else
TARGETS += blake3/blake3_sse2.o
ASM_TARGETS += blake3/blake3_sse2_x86-64_unix.S
endif

ifdef BLAKE3_NO_SSE41
EXTRAFLAGS += -DBLAKE3_NO_SSE41
else
TARGETS += blake3/blake3_sse41.o
ASM_TARGETS += blake3/blake3_sse41_x86-64_unix.S
endif

ifdef BLAKE3_NO_AVX2
EXTRAFLAGS += -DBLAKE3_NO_AVX2
else
TARGETS += blake3/blake3_avx2.o
ASM_TARGETS += blake3/blake3_avx2_x86-64_unix.S
endif

ifdef BLAKE3_NO_AVX512
EXTRAFLAGS += -DBLAKE3_NO_AVX512
else
TARGETS += blake3/blake3_avx512.o
ASM_TARGETS += blake3/blake3_avx512_x86-64_unix.S
endif

ifdef BLAKE3_USE_NEON
EXTRAFLAGS += -DBLAKE3_USE_NEON=1
TARGETS += blake3/blake3_neon.o
endif

ifdef BLAKE3_NO_NEON
EXTRAFLAGS += -DBLAKE3_USE_NEON=0
endif

all: blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c main.c table2.c $(TARGETS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $^ -o $(NAME) $(LDFLAGS)

blake3_sse2.o: blake3/blake3_sse2.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -msse2

blake3_sse41.o: blake3/blake3_sse41.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -msse4.1

blake3_avx2.o: blake3/blake3_avx2.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -mavx2

blake3_avx512.o: blake3/blake3_avx512.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -mavx512f -mavx512vl

blake3_neon.o: blake3/blake3_neon.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@

test: CFLAGS += -DBLAKE3_TESTING -fsanitize=address,undefined
test: all
	./test.py

asm: blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c main.c table2.c $(ASM_TARGETS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $^ -o $(NAME) $(LDFLAGS)

test_asm: CFLAGS += -DBLAKE3_TESTING -fsanitize=address,undefined 
test_asm: asm
	./test.py

vaultx_x86: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c $(ASM_TARGETS)
	$(CCP) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -x c++ -std=c++17 -o vaultx $(LDFLAGS) -fopenmp -ltbb

vaultx_x86_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c $(ASM_TARGETS)
	$(CC) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) -I/usr/include $(CFLAGS) $(EXTRAFLAGS) $^ -o vaultx $(LDFLAGS) -fopenmp 

vaultx_x86_xgcc: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c $(ASM_TARGETS)
	$(XCC) -I/ssd-raid0/shared/xgcc/include/ -I/ssd-raid0/shared/xgcc/lib/gcc/x86_64-pc-linux-gnu/12.2.1/include/ -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -o $@ $(LDFLAGS) -fopenmp 

vaultx_arm: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c
	$(CCP) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -x c++ -std=c++17 -o vaultx $(LDFLAGS) -fopenmp -ltbb 

vaultx_arm_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c
	$(CC) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -o vaultx $(LDFLAGS) -fopenmp


vaultx_mac: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c
#-D NONCE_SIZE=$(NONCE_SIZE)
	$(CCP) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $^ -x c++ -std=c++17 -o vaultx -fopenmp -O3 -lblake3 -lsodium -ltbb -I/opt/homebrew/opt/blake3/include -L/opt/homebrew/opt/blake3/lib -I/opt/homebrew/opt/tbb/include -L/opt/homebrew/opt/tbb/lib -I/opt/homebrew/opt/libsodium/include -L/opt/homebrew/opt/libsodium/lib

vaultx_mac_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c
#-D NONCE_SIZE=$(NONCE_SIZE)
	$(CC) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $^ -o vaultx -fopenmp -O3 -lblake3 -lsodium -I/opt/homebrew/opt/blake3/include -L/opt/homebrew/opt/blake3/lib -I/opt/homebrew/opt/libsodium/include -L/opt/homebrew/opt/libsodium/lib

blake3_bench: src/blake3_bench.c blake3/blake3.c blake3/blake3_dispatch.c blake3/blake3_portable.c $(ASM_TARGETS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -I/usr/include $(CFLAGS) $(EXTRAFLAGS) $^ -o blake3_bench $(LDFLAGS) -fopenmp

clean: 
	rm -f $(NAME) vaultx vaultx_* *.o
