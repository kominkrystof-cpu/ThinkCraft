#ifndef RUNTIME_H
#define RUNTIME_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t length;
    uint32_t reserved;
} JArray;

void rt_arena_init(size_t capacity);
void *rt_alloc(size_t size);
JArray *rt_new_float_array(uint32_t length);

#endif
