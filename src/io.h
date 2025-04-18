#ifndef IO_H
#define IO_H

#include "globals.h"

int rename_file(const char *old_name, const char *new_name);
void remove_file(const char *fileName);
int move_file_overwrite(const char *source_path, const char *destination_path);

#endif