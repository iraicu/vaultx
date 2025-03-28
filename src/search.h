#ifndef SEARCH_H
#define SEARCH_H

#include "globals.h"

void search_memo_records(const char *filename, const char *SEARCH_STRING);
void search_memo_records_batch(const char *filename, int num_lookups, int search_size, int num_threads);

#endif