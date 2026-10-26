#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned char *arena;
static size_t arena_capacity;
static size_t arena_used;

void rt_arena_init(size_t capacity) {
    if (arena != NULL) {
        free(arena);
    }

    arena = calloc(1, capacity);
    if (arena == NULL) {
        perror("rt_arena_init");
        abort();
    }

    arena_capacity = capacity;
    arena_used = 0;
}

void *rt_alloc(size_t size) {
    size_t aligned = (size + 15) & ~(size_t)15;
    void *result;

    if (arena == NULL) {
        rt_arena_init(64 * 1024 * 1024);
    }

    if (aligned > arena_capacity - arena_used) {
        fprintf(stderr, "rt_alloc: arena exhausted\n");
        abort();
    }

    result = arena + arena_used;
    arena_used += aligned;
    return result;
}
