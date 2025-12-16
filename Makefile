NAME=blake3/c/blake3
CC ?= gcc
CCP ?= g++

BLAKE3_DIR = blake3/c

# On macOS prefer clang and ensure the macOS SDK is used so system headers
# like <dirent.h> and the fixed includes under sys/_types are found.
ifeq ($(shell uname -s),Darwin)
	# On macOS prefer a Homebrew-provided LLVM/clang (if installed) with libomp,
	# because Apple's clang doesn't ship OpenMP and Homebrew gcc needs the
	# SDK path manually. If Homebrew LLVM is present, use it and point to
	# libomp include/lib paths; otherwise fall back to Homebrew gcc and add
	# the macOS SDK sysroot so system headers are found.
	ifneq ($(wildcard /opt/homebrew/opt/llvm/bin/clang),)
		CC = /opt/homebrew/opt/llvm/bin/clang
		CCP = /opt/homebrew/opt/llvm/bin/clang++
		CFLAGS += -isysroot $(shell xcrun --show-sdk-path) -I/opt/homebrew/opt/libomp/include
		LDFLAGS += -L/opt/homebrew/opt/libomp/lib -Wl,-rpath,/opt/homebrew/opt/libomp/lib
		EXTRAFLAGS += -fopenmp
	else
		CC = gcc-13
		CCP = g++-14
		CFLAGS += -isysroot $(shell xcrun --show-sdk-path)
	endif
endif
#torus
#XCC=/ssd-raid0/shared/xgcc/bin/xgcc
#s8
XCC=/home/wwang/xgcc/bin/xgcc

CFLAGS=-O3 -DBLAKE3_USE_NEON=0 -Wall -Wextra -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -fvisibility=hidden
LDFLAGS = -lm -lpthread -pie -Wl,-z,relro,-z,now -lsodium -lnuma
TARGETS=
ASM_TARGETS=
EXTRAFLAGS=-Wa,--noexecstack

# You can set values with a default, but allow it to be overridden
NONCE_SIZE ?= 4
RECORD_SIZE ?= 12

ifdef BLAKE3_NO_SSE2
EXTRAFLAGS += -DBLAKE3_NO_SSE2
else
TARGETS += $(BLAKE3_DIR)/blake3_sse2.o
ASM_TARGETS += $(BLAKE3_DIR)/blake3_sse2_x86-64_unix.S
endif

ifdef BLAKE3_NO_SSE41
EXTRAFLAGS += -DBLAKE3_NO_SSE41
else
TARGETS += $(BLAKE3_DIR)/blake3_sse41.o
ASM_TARGETS += $(BLAKE3_DIR)/blake3_sse41_x86-64_unix.S
endif

ifdef BLAKE3_NO_AVX2
EXTRAFLAGS += -DBLAKE3_NO_AVX2
else
TARGETS += $(BLAKE3_DIR)/blake3_avx2.o
ASM_TARGETS += $(BLAKE3_DIR)/blake3_avx2_x86-64_unix.S
endif

ifdef BLAKE3_NO_AVX512
EXTRAFLAGS += -DBLAKE3_NO_AVX512
else
TARGETS += $(BLAKE3_DIR)/blake3_avx512.o
ASM_TARGETS += $(BLAKE3_DIR)/blake3_avx512_x86-64_unix.S
endif

ifdef BLAKE3_USE_NEON
EXTRAFLAGS += -DBLAKE3_USE_NEON=1
TARGETS += $(BLAKE3_DIR)/blake3_neon.o
endif

ifdef BLAKE3_NO_NEON
EXTRAFLAGS += -DBLAKE3_USE_NEON=0
endif

.PHONY: submodule-init
submodule-init:
	@if [ ! -f $(BLAKE3_DIR)/blake3.c ]; then \
		echo "Initializing blake3 submodule..."; \
		git submodule update --init --recursive; \
	fi

all: $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c main.c table2.c src/utils.c $(TARGETS) | submodule-init
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $^ -o $(NAME) $(LDFLAGS)

$(BLAKE3_DIR)/blake3_sse2.o: $(BLAKE3_DIR)/blake3_sse2.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -msse2

$(BLAKE3_DIR)/blake3_sse41.o: $(BLAKE3_DIR)/blake3_sse41.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -msse4.1

$(BLAKE3_DIR)/blake3_avx2.o: $(BLAKE3_DIR)/blake3_avx2.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -mavx2

$(BLAKE3_DIR)/blake3_avx512.o: $(BLAKE3_DIR)/blake3_avx512.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@ -mavx512f -mavx512vl

$(BLAKE3_DIR)/blake3_neon.o: $(BLAKE3_DIR)/blake3_neon.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $^ -o $@

test: CFLAGS += -DBLAKE3_TESTING -fsanitize=address,undefined
test: all
	./test.py

asm: $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c main.c table2.c $(ASM_TARGETS) | submodule-init
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $^ -o $(NAME) $(LDFLAGS)

test_asm: CFLAGS += -DBLAKE3_TESTING -fsanitize=address,undefined
test_asm: asm
	./test.py

vaultx_x86: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c $(ASM_TARGETS) src/utils.c | submodule-init
	$(CCP) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -x c++ -std=c++17 -o vaultx $(LDFLAGS) -fopenmp -ltbb

vaultx_x86_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c $(ASM_TARGETS) src/utils.c | submodule-init
	$(CC) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) -I/usr/include $(CFLAGS) $(EXTRAFLAGS) $^ -o vaultx $(LDFLAGS) -fopenmp

vaultx_x86_xgcc: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c $(ASM_TARGETS) src/utils.c | submodule-init
	$(XCC) -I/ssd-raid0/shared/xgcc/include/ -I/ssd-raid0/shared/xgcc/lib/gcc/x86_64-pc-linux-gnu/12.2.1/include/ -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -o $@ $(LDFLAGS) -fopenmp

vaultx_arm: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c src/utils.c | submodule-init
	$(CCP) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -x c++ -std=c++17 -o vaultx $(LDFLAGS) -fopenmp -ltbb

vaultx_arm_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c $(BLAKE3_DIR)/blake3.c $(BLAKE3_DIR)/blake3_dispatch.c $(BLAKE3_DIR)/blake3_portable.c src/utils.c | submodule-init
	$(CC) -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $(CFLAGS) $(EXTRAFLAGS) $^ -o vaultx $(LDFLAGS) -fopenmp

vaultx_mac: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c src/utils.c
	$(CCP) $(CFLAGS) $(EXTRAFLAGS) -w -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $^ -x c++ -std=c++17 -o vaultx -fopenmp -ltbb -lblake3 -lsodium -O3 -I/opt/homebrew/include -I/opt/homebrew/opt/blake3/include -I/opt/homebrew/opt/tbb/include -L/opt/homebrew/lib -L/opt/homebrew/opt/blake3/lib -L/opt/homebrew/opt/tbb/lib

vaultx_mac_c: src/vaultx.c src/table1.c src/sort.c src/table2.c src/shuffle.c src/globals.c src/io.c src/search.c src/crypto.c src/merge.c src/utils.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -w -DNONCE_SIZE=$(NONCE_SIZE) -DRECORD_SIZE=$(RECORD_SIZE) $^ -O3 -o vaultx -fopenmp -lblake3 -lsodium -I/opt/homebrew/include -I/opt/homebrew/opt/blake3/include -L/opt/homebrew/lib -L/opt/homebrew/opt/blake3/lib

clean:
	rm -f $(NAME) vaultx vaultx_* *.o $(BLAKE3_DIR)/*.o
