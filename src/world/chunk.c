#include "thinkcraft/chunk.h"
#include "thinkcraft/terrain.h"

#include <string.h>

static const int neighbor_offsets[6][3] = {
    { 1,  0,  0},
    {-1,  0,  0},
    { 0,  1,  0},
    { 0, -1,  0},
    { 0,  0,  1},
    { 0,  0, -1}
};

void tc_chunk_init(tc_chunk *chunk, int32_t origin_x, int32_t origin_y,
                   int32_t origin_z) {
    if (chunk == NULL) {
        return;
    }

    chunk->origin_x = origin_x;
    chunk->origin_y = origin_y;
    chunk->origin_z = origin_z;
    tc_chunk_clear(chunk);
}

void tc_chunk_clear(tc_chunk *chunk) {
    if (chunk == NULL) {
        return;
    }

    memset(chunk->blocks, TC_BLOCK_AIR, sizeof(chunk->blocks));
    chunk->solid_count = 0;
}

int tc_chunk_set(tc_chunk *chunk, int x, int y, int z, uint8_t block) {
    size_t index;
    uint8_t previous;

    if (chunk == NULL) {
        return 0;
    }

    if ((unsigned)x >= TC_CHUNK_SIZE_X
        || (unsigned)y >= TC_CHUNK_SIZE_Y
        || (unsigned)z >= TC_CHUNK_SIZE_Z) {
        return 0;
    }

    index = tc_chunk_index(x, y, z);
    previous = chunk->blocks[index];

    if (previous == TC_BLOCK_AIR && block != TC_BLOCK_AIR) {
        chunk->solid_count++;
    } else if (previous != TC_BLOCK_AIR && block == TC_BLOCK_AIR) {
        chunk->solid_count--;
    }

    chunk->blocks[index] = block;
    return 1;
}

void tc_chunk_generate(tc_chunk *chunk, uint32_t seed) {
    tc_terrain_generate_chunk(chunk, seed);
}

uint32_t tc_chunk_visible_face_count_sampled(const tc_chunk *chunk,
                                              tc_block_sampler sampler,
                                              const void *context) {
    uint32_t faces = 0;

    if (chunk == NULL) {
        return 0;
    }

    for (int y = 0; y < TC_CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
            for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
                uint8_t block = chunk->blocks[tc_chunk_index(x, y, z)];
                if (!tc_block_is_solid(block)) {
                    continue;
                }

                for (int direction = 0; direction < 6; direction++) {
                    int neighbor_x = x + neighbor_offsets[direction][0];
                    int neighbor_y = y + neighbor_offsets[direction][1];
                    int neighbor_z = z + neighbor_offsets[direction][2];
                    uint8_t neighbor;

                    if (sampler != NULL) {
                        neighbor = sampler(context, neighbor_x, neighbor_y,
                                           neighbor_z);
                    } else {
                        neighbor = tc_chunk_get(chunk, neighbor_x, neighbor_y,
                                                neighbor_z);
                    }

                    if (!tc_block_is_solid(neighbor)) {
                        faces++;
                    }
                }
            }
        }
    }

    return faces;
}

uint32_t tc_chunk_visible_face_count(const tc_chunk *chunk) {
    return tc_chunk_visible_face_count_sampled(chunk, NULL, NULL);
}
