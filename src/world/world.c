#include "thinkcraft/world.h"

#include <math.h>
#include <string.h>

typedef struct {
    const tc_world *world;
    const tc_chunk *chunk;
} world_sample_context;

static int terrain_height(int x, int z, int center_x, int center_z) {
    float wave = 2.2f * sinf((float)x * 0.13f)
               + 1.8f * cosf((float)z * 0.11f)
               + 1.0f * sinf((float)(x + z) * 0.07f);
    int delta_x = x - center_x;
    int delta_z = z - center_z;
    int distance_squared = delta_x * delta_x + delta_z * delta_z;
    int height = 12 + (int)wave;

    if (distance_squared < 225) {
        int hill = 6 - distance_squared / 45;
        if (hill > 0) {
            height += hill;
        }
    }

    if (height < 7) {
        height = 7;
    }
    if (height > 26) {
        height = 26;
    }

    return height;
}

static uint8_t sample_chunk_block(const void *context, int x, int y, int z) {
    const world_sample_context *sample = context;

    if ((unsigned)x < TC_CHUNK_SIZE_X
        && (unsigned)y < TC_CHUNK_SIZE_Y
        && (unsigned)z < TC_CHUNK_SIZE_Z) {
        return tc_chunk_get(sample->chunk, x, y, z);
    }

    return tc_world_get_block(
        sample->world,
        sample->chunk->origin_x + x,
        sample->chunk->origin_y + y,
        sample->chunk->origin_z + z);
}

int tc_world_init(tc_world *world, tc_arena *arena, int chunks_x, int chunks_z) {
    size_t chunk_count;
    size_t bytes;
    tc_chunk *chunks;
    size_t index = 0;

    if (world == NULL || arena == NULL || chunks_x <= 0 || chunks_z <= 0) {
        return 0;
    }

    chunk_count = (size_t)chunks_x * (size_t)chunks_z;
    if (chunk_count > SIZE_MAX / sizeof(tc_chunk)) {
        return 0;
    }

    bytes = chunk_count * sizeof(tc_chunk);
    chunks = tc_arena_alloc(arena, bytes);
    if (chunks == NULL) {
        return 0;
    }

    memset(chunks, 0, bytes);
    world->chunks_x = chunks_x;
    world->chunks_z = chunks_z;
    world->chunks = chunks;

    for (int chunk_z = 0; chunk_z < chunks_z; chunk_z++) {
        for (int chunk_x = 0; chunk_x < chunks_x; chunk_x++) {
            tc_chunk_init(&chunks[index],
                          chunk_x * TC_CHUNK_SIZE_X,
                          0,
                          chunk_z * TC_CHUNK_SIZE_Z);
            index++;
        }
    }

    return 1;
}

void tc_world_shutdown(tc_world *world) {
    if (world != NULL) {
        memset(world, 0, sizeof(*world));
    }
}

tc_chunk *tc_world_chunk_at(const tc_world *world, int chunk_x, int chunk_z) {
    if (world == NULL
        || chunk_x < 0 || chunk_x >= world->chunks_x
        || chunk_z < 0 || chunk_z >= world->chunks_z) {
        return NULL;
    }

    return &world->chunks[(size_t)chunk_z * (size_t)world->chunks_x
                          + (size_t)chunk_x];
}

uint8_t tc_world_get_block(const tc_world *world, int x, int y, int z) {
    int chunk_x;
    int chunk_z;
    const tc_chunk *chunk;

    if (world == NULL || x < 0 || z < 0
        || y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return TC_BLOCK_AIR;
    }

    chunk_x = x / TC_CHUNK_SIZE_X;
    chunk_z = z / TC_CHUNK_SIZE_Z;
    chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    if (chunk == NULL) {
        return TC_BLOCK_AIR;
    }

    return tc_chunk_get(chunk, x % TC_CHUNK_SIZE_X, y, z % TC_CHUNK_SIZE_Z);
}

int tc_world_set_block(tc_world *world, int x, int y, int z, uint8_t block) {
    int chunk_x;
    int chunk_z;
    tc_chunk *chunk;

    if (world == NULL || x < 0 || z < 0
        || y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    chunk_x = x / TC_CHUNK_SIZE_X;
    chunk_z = z / TC_CHUNK_SIZE_Z;
    chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    if (chunk == NULL) {
        return 0;
    }

    return tc_chunk_set(chunk, x % TC_CHUNK_SIZE_X, y, z % TC_CHUNK_SIZE_Z,
                        block);
}

void tc_world_generate_demo(tc_world *world) {
    int center_x;
    int center_z;

    if (world == NULL) {
        return;
    }

    center_x = world->chunks_x * TC_CHUNK_SIZE_X / 2;
    center_z = world->chunks_z * TC_CHUNK_SIZE_Z / 2;

    for (int chunk_z = 0; chunk_z < world->chunks_z; chunk_z++) {
        for (int chunk_x = 0; chunk_x < world->chunks_x; chunk_x++) {
            tc_chunk *chunk = tc_world_chunk_at(world, chunk_x, chunk_z);

            tc_chunk_clear(chunk);

            for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
                for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
                    int world_x = chunk->origin_x + x;
                    int world_z = chunk->origin_z + z;
                    int height = terrain_height(world_x, world_z,
                                                center_x, center_z);
                    uint8_t top = TC_BLOCK_GRASS;

                    if (height >= 20) {
                        top = TC_BLOCK_STONE;
                    } else if (world_x < 8 && world_z < 8) {
                        top = TC_BLOCK_SAND;
                    }

                    for (int y = 0; y <= height; y++) {
                        uint8_t block;

                        if (y == height) {
                            block = top;
                        } else if (top == TC_BLOCK_STONE) {
                            block = TC_BLOCK_STONE;
                        } else if (top == TC_BLOCK_SAND
                                   && y >= height - 2) {
                            block = TC_BLOCK_SAND;
                        } else if (y >= height - 3) {
                            block = TC_BLOCK_DIRT;
                        } else {
                            block = TC_BLOCK_STONE;
                        }

                        tc_chunk_set(chunk, x, y, z, block);
                    }
                }
            }

            chunk->solid_count = tc_chunk_count_solid(chunk->blocks,
                                                      TC_CHUNK_BLOCK_COUNT);
        }
    }
}

int tc_world_build_chunk_mesh(const tc_world *world, int chunk_x, int chunk_z,
                              tc_mesh *mesh, tc_mesh_stats *stats) {
    const tc_chunk *chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    world_sample_context context;

    if (chunk == NULL) {
        return 0;
    }

    context.world = world;
    context.chunk = chunk;

    return tc_chunk_build_mesh_sampled(chunk, sample_chunk_block, &context,
                                       mesh, stats);
}
