#ifndef THINKCRAFT_ARENA_H
#define THINKCRAFT_ARENA_H

#include <stddef.h>

typedef struct {
    unsigned char *memory;
    size_t capacity;
    size_t used;
} tc_arena;

int tc_arena_init(tc_arena *arena, size_t capacity);
void tc_arena_shutdown(tc_arena *arena);
void *tc_arena_alloc(tc_arena *arena, size_t size);
char *tc_arena_strdup(tc_arena *arena, const char *text);

#endif
