#include "thinkcraft/chunk.h"

#include <math.h>
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

void tc_chunk_generate_demo(tc_chunk *chunk) {
    if (chunk == NULL) {
        return;
    }

    tc_chunk_clear(chunk);

    for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
        for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
            float wave = 2.5f * sinf((float)x * 0.55f)
                       + 2.0f * cosf((float)z * 0.47f);
            int height = 12 + (int)wave;

            if (height < 8) {
                height = 8;
            }
            if (height > 20) {
                height = 20;
            }

            for (int y = 0; y <= height; y++) {
                uint8_t block;

                if (y == height) {
                    block = (x < 4 && z < 4) ? TC_BLOCK_SAND : TC_BLOCK_GRASS;
                } else if (y >= height - 3) {
                    block = TC_BLOCK_DIRT;
                } else {
                    block = TC_BLOCK_STONE;
                }

                tc_chunk_set(chunk, x, y, z, block);
            }

            if (x >= 10 && x <= 13 && z >= 8 && z <= 11) {
                int extra = 2;
                if (x == 11 || x == 12) {
                    extra++;
                }
                if (z == 9 || z == 10) {
                    extra++;
                }

                for (int y = height + 1;
                     y <= height + extra && y < TC_CHUNK_SIZE_Y;
                     y++) {
                    tc_chunk_set(chunk, x, y, z, TC_BLOCK_STONE);
                }
            }
        }
    }

    chunk->solid_count = tc_chunk_count_solid(chunk->blocks,
                                              TC_CHUNK_BLOCK_COUNT);
}

uint32_t tc_chunk_visible_face_count(const tc_chunk *chunk) {
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
                    uint8_t neighbor = tc_chunk_get(
                        chunk,
                        x + neighbor_offsets[direction][0],
                        y + neighbor_offsets[direction][1],
                        z + neighbor_offsets[direction][2]);

                    if (!tc_block_is_solid(neighbor)) {
                        faces++;
                    }
                }
            }
        }
    }

    return faces;
}
