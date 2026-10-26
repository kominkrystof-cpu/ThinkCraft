#ifndef THINKCRAFT_CHUNK_H
#define THINKCRAFT_CHUNK_H

#include <stddef.h>
#include <stdint.h>

#include "thinkcraft/block.h"

#define TC_CHUNK_SIZE_X 16
#define TC_CHUNK_SIZE_Y 256
#define TC_CHUNK_SIZE_Z 16

#define TC_CHUNK_BLOCK_COUNT \
    (TC_CHUNK_SIZE_X * TC_CHUNK_SIZE_Y * TC_CHUNK_SIZE_Z)

typedef struct {
    int32_t origin_x;
    int32_t origin_y;
    int32_t origin_z;
    uint32_t solid_count;
    uint8_t blocks[TC_CHUNK_BLOCK_COUNT];
} tc_chunk;

static inline size_t tc_chunk_index(int x, int y, int z) {
    return (((size_t)y * TC_CHUNK_SIZE_Z + (size_t)z) * TC_CHUNK_SIZE_X
            + (size_t)x);
}

static inline uint8_t tc_chunk_get(const tc_chunk *chunk, int x, int y, int z) {
    if ((unsigned)x >= TC_CHUNK_SIZE_X
        || (unsigned)y >= TC_CHUNK_SIZE_Y
        || (unsigned)z >= TC_CHUNK_SIZE_Z) {
        return TC_BLOCK_AIR;
    }

    return chunk->blocks[tc_chunk_index(x, y, z)];
}

void tc_chunk_init(tc_chunk *chunk, int32_t origin_x, int32_t origin_y,
                   int32_t origin_z);
void tc_chunk_clear(tc_chunk *chunk);
int tc_chunk_set(tc_chunk *chunk, int x, int y, int z, uint8_t block);
void tc_chunk_generate_demo(tc_chunk *chunk);
uint32_t tc_chunk_visible_face_count(const tc_chunk *chunk);

uint32_t tc_chunk_count_solid(const uint8_t *blocks, size_t count);

#endif
