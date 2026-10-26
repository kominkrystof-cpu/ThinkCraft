#ifndef THINKCRAFT_PLATFORM_H
#define THINKCRAFT_PLATFORM_H

#include <stddef.h>
#include "thinkcraft/arena.h"

int tc_executable_dir(char *buffer, size_t size);
int tc_path_exists(const char *path);
char *tc_read_text_file(const char *path, tc_arena *arena);
double tc_time_now(void);
void tc_sleep_ms(double milliseconds);

#endif
