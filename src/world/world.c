#include "thinkcraft/world.h"
#include "thinkcraft/math.h"

#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct {
    const tc_world *world;
    const tc_chunk *chunk;
} world_sample_context;

static int terrain_height(int x, int z) {
    float wave = 2.2f * sinf((float)x * 0.13f)
               + 1.8f * cosf((float)z * 0.11f)
               + 1.0f * sinf((float)(x + z) * 0.07f);
    int delta_x = x - 8;
    int delta_z = z - 8;
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

static void generate_chunk_demo(tc_chunk *chunk) {
    tc_chunk_clear(chunk);

    for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
        for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
            int world_x = chunk->origin_x + x;
            int world_z = chunk->origin_z + z;
            int height = terrain_height(world_x, world_z);
            uint8_t top = TC_BLOCK_GRASS;

            if (height >= 23) {
                top = TC_BLOCK_STONE;
            } else if (world_x >= -8 && world_x < 4
                       && world_z >= -8 && world_z < 4) {
                top = TC_BLOCK_SAND;
            }

            for (int y = 0; y <= height; y++) {
                uint8_t block;

                if (y == height) {
                    block = top;
                } else if (top == TC_BLOCK_STONE) {
                    block = TC_BLOCK_STONE;
                } else if (top == TC_BLOCK_SAND && y >= height - 2) {
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

static size_t slot_index(const tc_world *world, int chunk_x, int chunk_z) {
    int x = tc_math_floor_mod(chunk_x, world->size);
    int z = tc_math_floor_mod(chunk_z, world->size);

    return (size_t)z * (size_t)world->size + (size_t)x;
}

static tc_world_slot *slot_for_coord(const tc_world *world, int chunk_x,
                                     int chunk_z) {
    tc_world_slot *slot;

    if (world == NULL || world->slots == NULL || world->size <= 0) {
        return NULL;
    }

    slot = &world->slots[slot_index(world, chunk_x, chunk_z)];
    if (!slot->loaded || slot->chunk_x != chunk_x
        || slot->chunk_z != chunk_z) {
        return NULL;
    }

    return slot;
}

static void mark_chunk_dirty(tc_world *world, int chunk_x, int chunk_z) {
    tc_world_slot *slot = slot_for_coord(world, chunk_x, chunk_z);

    if (slot != NULL) {
        slot->dirty = 1;
    }
}

static void mark_neighbor_chunks_dirty(tc_world *world, int chunk_x,
                                       int chunk_z) {
    mark_chunk_dirty(world, chunk_x + 1, chunk_z);
    mark_chunk_dirty(world, chunk_x - 1, chunk_z);
    mark_chunk_dirty(world, chunk_x, chunk_z + 1);
    mark_chunk_dirty(world, chunk_x, chunk_z - 1);
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

int tc_world_init(tc_world *world, tc_arena *arena, int radius) {
    size_t slot_count;
    size_t bytes;
    int size;

    if (world == NULL || arena == NULL || radius < 0
        || radius > (INT_MAX - 1) / 2) {
        return 0;
    }

    size = radius * 2 + 1;
    slot_count = (size_t)size * (size_t)size;
    if (slot_count > SIZE_MAX / sizeof(tc_world_slot)) {
        return 0;
    }

    bytes = slot_count * sizeof(tc_world_slot);
    memset(world, 0, sizeof(*world));
    world->radius = radius;
    world->size = size;
    world->slots = tc_arena_alloc(arena, bytes);
    if (world->slots == NULL) {
        return 0;
    }

    memset(world->slots, 0, bytes);

    return tc_world_update(world, 0, 0);
}

void tc_world_shutdown(tc_world *world) {
    if (world != NULL) {
        memset(world, 0, sizeof(*world));
    }
}

int tc_world_update(tc_world *world, int center_chunk_x, int center_chunk_z) {
    int radius;

    if (world == NULL || world->slots == NULL || world->size <= 0) {
        return 0;
    }

    world->center_chunk_x = center_chunk_x;
    world->center_chunk_z = center_chunk_z;
    radius = world->radius;

    for (int index = 0; index < world->size * world->size; index++) {
        tc_world_slot *slot = &world->slots[index];

        if (!slot->loaded) {
            continue;
        }

        if (slot->chunk_x < center_chunk_x - radius
            || slot->chunk_x > center_chunk_x + radius
            || slot->chunk_z < center_chunk_z - radius
            || slot->chunk_z > center_chunk_z + radius) {
            mark_neighbor_chunks_dirty(world, slot->chunk_x, slot->chunk_z);
            slot->loaded = 0;
            slot->dirty = 0;
        }
    }

    for (int offset_z = -radius; offset_z <= radius; offset_z++) {
        for (int offset_x = -radius; offset_x <= radius; offset_x++) {
            int chunk_x = center_chunk_x + offset_x;
            int chunk_z = center_chunk_z + offset_z;
            tc_world_slot *slot = &world->slots[slot_index(world, chunk_x,
                                                           chunk_z)];

            if (slot->loaded) {
                if (slot->chunk_x == chunk_x && slot->chunk_z == chunk_z) {
                    continue;
                }

                mark_neighbor_chunks_dirty(world, slot->chunk_x,
                                           slot->chunk_z);
                slot->loaded = 0;
                slot->dirty = 0;
            }

            tc_chunk_init(&slot->chunk,
                          chunk_x * TC_CHUNK_SIZE_X,
                          0,
                          chunk_z * TC_CHUNK_SIZE_Z);
            slot->chunk_x = chunk_x;
            slot->chunk_z = chunk_z;
            slot->loaded = 1;
            slot->dirty = 1;

            if (world->generate_demo) {
                generate_chunk_demo(&slot->chunk);
            }

            mark_neighbor_chunks_dirty(world, chunk_x, chunk_z);
        }
    }

    return 1;
}

tc_chunk *tc_world_chunk_at(const tc_world *world, int chunk_x, int chunk_z) {
    tc_world_slot *slot = slot_for_coord(world, chunk_x, chunk_z);

    return slot != NULL ? &slot->chunk : NULL;
}

uint8_t tc_world_get_block(const tc_world *world, int x, int y, int z) {
    int chunk_x;
    int chunk_z;
    const tc_chunk *chunk;

    if (world == NULL || y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return TC_BLOCK_AIR;
    }

    chunk_x = tc_math_floor_div(x, TC_CHUNK_SIZE_X);
    chunk_z = tc_math_floor_div(z, TC_CHUNK_SIZE_Z);
    chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    if (chunk == NULL) {
        return TC_BLOCK_AIR;
    }

    return tc_chunk_get(chunk,
                        tc_math_floor_mod(x, TC_CHUNK_SIZE_X),
                        y,
                        tc_math_floor_mod(z, TC_CHUNK_SIZE_Z));
}

int tc_world_set_block(tc_world *world, int x, int y, int z, uint8_t block) {
    int chunk_x;
    int chunk_z;
    int local_x;
    int local_z;
    tc_chunk *chunk;
    tc_world_slot *slot;

    if (world == NULL || y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    chunk_x = tc_math_floor_div(x, TC_CHUNK_SIZE_X);
    chunk_z = tc_math_floor_div(z, TC_CHUNK_SIZE_Z);
    local_x = tc_math_floor_mod(x, TC_CHUNK_SIZE_X);
    local_z = tc_math_floor_mod(z, TC_CHUNK_SIZE_Z);
    chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    if (chunk == NULL) {
        return 0;
    }

    if (!tc_chunk_set(chunk, local_x, y, local_z, block)) {
        return 0;
    }

    slot = slot_for_coord(world, chunk_x, chunk_z);
    if (slot != NULL) {
        slot->dirty = 1;
    }

    if (local_x == 0) {
        mark_chunk_dirty(world, chunk_x - 1, chunk_z);
    }
    if (local_x == TC_CHUNK_SIZE_X - 1) {
        mark_chunk_dirty(world, chunk_x + 1, chunk_z);
    }
    if (local_z == 0) {
        mark_chunk_dirty(world, chunk_x, chunk_z - 1);
    }
    if (local_z == TC_CHUNK_SIZE_Z - 1) {
        mark_chunk_dirty(world, chunk_x, chunk_z + 1);
    }

    return 1;
}

void tc_world_generate_demo(tc_world *world) {
    if (world == NULL || world->slots == NULL) {
        return;
    }

    world->generate_demo = 1;

    for (int index = 0; index < world->size * world->size; index++) {
        tc_world_slot *slot = &world->slots[index];

        if (!slot->loaded) {
            continue;
        }

        generate_chunk_demo(&slot->chunk);
        slot->dirty = 1;
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
