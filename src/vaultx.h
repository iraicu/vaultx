#ifndef VAULTX_H
#define VAULTX_H

#ifdef __linux__
#include <linux/fs.h> // Provides `syncfs` on Linux
#endif

#ifdef __cplusplus
// Your C++-specific code here
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif

#include "globals.h"

uint64_t largest_power_of_two_less_than(uint64_t number);

#endif // VAULTX_H