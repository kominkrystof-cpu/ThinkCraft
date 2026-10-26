#include "thinkcraft/arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tc_arena_init(tc_arena *arena, size_t capacity) {
    if (arena == NULL || capacity == 0) {
        return 0;
    }

    arena->memory = calloc(1, capacity);
    if (arena->memory == NULL) {
        perror("tc_arena_init");
        return 0;
    }

    arena->capacity = capacity;
    arena->used = 0;
    return 1;
}

void tc_arena_shutdown(tc_arena *arena) {
    if (arena == NULL) {
        return;
    }

    free(arena->memory);
    arena->memory = NULL;
    arena->capacity = 0;
    arena->used = 0;
}

void *tc_arena_alloc(tc_arena *arena, size_t size) {
    size_t aligned;
    void *result;

    if (arena == NULL || arena->memory == NULL) {
        return NULL;
    }

    aligned = (size + 15) & ~(size_t)15;
    if (aligned > arena->capacity - arena->used) {
        fprintf(stderr, "tc_arena_alloc: arena exhausted\n");
        return NULL;
    }

    result = arena->memory + arena->used;
    arena->used += aligned;
    return result;
}

char *tc_arena_strdup(tc_arena *arena, const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = tc_arena_alloc(arena, length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}
