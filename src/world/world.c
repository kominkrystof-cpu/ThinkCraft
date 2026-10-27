#include "thinkcraft/world.h"
#include "thinkcraft/math.h"
#include "thinkcraft/terrain.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const tc_world *world;
    const tc_chunk *chunk;
} world_sample_context;

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

static void mark_block_neighbors_dirty(tc_world *world, int chunk_x,
                                       int chunk_z, int local_x,
                                       int local_z) {
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
}

static int ensure_edit_capacity(tc_world *world, size_t needed) {
    size_t capacity;
    tc_world_edit *edits;

    if (needed <= world->edit_capacity) {
        return 1;
    }

    capacity = world->edit_capacity > 0 ? world->edit_capacity : 256;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(*edits)) {
        return 0;
    }

    edits = realloc(world->edits, capacity * sizeof(*edits));
    if (edits == NULL) {
        return 0;
    }

    world->edits = edits;
    world->edit_capacity = capacity;
    return 1;
}

static int append_edit(tc_world *world, int x, int y, int z, uint8_t block) {
    tc_world_edit *edit;

    if (!ensure_edit_capacity(world, world->edit_count + 1)) {
        return 0;
    }

    edit = &world->edits[world->edit_count++];
    edit->x = x;
    edit->y = y;
    edit->z = z;
    edit->block = block;
    return 1;
}

static void apply_edit_to_loaded_chunk(tc_world *world, int chunk_x,
                                       int chunk_z, int x, int y, int z,
                                       uint8_t block) {
    tc_chunk *chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
    int local_x;
    int local_z;
    tc_world_slot *slot;

    if (chunk == NULL) {
        return;
    }

    local_x = tc_math_floor_mod(x, TC_CHUNK_SIZE_X);
    local_z = tc_math_floor_mod(z, TC_CHUNK_SIZE_Z);
    if (!tc_chunk_set(chunk, local_x, y, local_z, block)) {
        return;
    }

    slot = slot_for_coord(world, chunk_x, chunk_z);
    if (slot != NULL) {
        slot->dirty = 1;
    }
    mark_block_neighbors_dirty(world, chunk_x, chunk_z, local_x, local_z);
}

static void apply_edits_to_chunk(tc_world *world, tc_chunk *chunk) {
    for (size_t index = 0; index < world->edit_count; index++) {
        const tc_world_edit *edit = &world->edits[index];
        int local_x;
        int local_z;

        if (edit->x < chunk->origin_x
            || edit->x >= chunk->origin_x + TC_CHUNK_SIZE_X
            || edit->z < chunk->origin_z
            || edit->z >= chunk->origin_z + TC_CHUNK_SIZE_Z) {
            continue;
        }

        local_x = (int)(edit->x - chunk->origin_x);
        local_z = (int)(edit->z - chunk->origin_z);
        tc_chunk_set(chunk, local_x, (int)edit->y, local_z, edit->block);
    }
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
    world->max_radius = radius;
    world->size = size;
    world->seed = TC_TERRAIN_DEFAULT_SEED;
    world->slots = tc_arena_alloc(arena, bytes);
    if (world->slots == NULL) {
        return 0;
    }

    memset(world->slots, 0, bytes);

    return tc_world_update(world, 0, 0);
}

void tc_world_shutdown(tc_world *world) {
    if (world != NULL) {
        free(world->edits);
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

            if (world->generate_terrain) {
                tc_chunk_generate(&slot->chunk, world->seed);
            }
            apply_edits_to_chunk(world, &slot->chunk);

            mark_neighbor_chunks_dirty(world, chunk_x, chunk_z);
        }
    }

    return 1;
}

int tc_world_set_radius(tc_world *world, int radius) {
    if (world == NULL || radius < 0 || radius > world->max_radius) {
        return 0;
    }

    if (world->radius != radius) {
        world->radius = radius;
    }

    return tc_world_update(world, world->center_chunk_x, world->center_chunk_z);
}

size_t tc_world_loaded_count(const tc_world *world) {
    size_t count = 0;

    if (world == NULL || world->slots == NULL) {
        return 0;
    }

    for (int index = 0; index < world->size * world->size; index++) {
        if (world->slots[index].loaded) {
            count++;
        }
    }

    return count;
}

size_t tc_world_capacity(const tc_world *world) {
    if (world == NULL || world->size <= 0) {
        return 0;
    }

    return (size_t)world->size * (size_t)world->size;
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

    if (tc_chunk_get(chunk, local_x, y, local_z) == block) {
        return 1;
    }

    if (tc_chunk_get(chunk, local_x, y, local_z) == TC_BLOCK_BEDROCK
        && block != TC_BLOCK_BEDROCK) {
        return 0;
    }

    return tc_world_add_edit(world, x, y, z, block);
}

int tc_world_add_edit(tc_world *world, int x, int y, int z, uint8_t block) {
    int chunk_x;
    int chunk_z;
    int local_x;
    int local_z;
    tc_chunk *chunk;

    if (world == NULL || y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    chunk_x = tc_math_floor_div(x, TC_CHUNK_SIZE_X);
    chunk_z = tc_math_floor_div(z, TC_CHUNK_SIZE_Z);
    local_x = tc_math_floor_mod(x, TC_CHUNK_SIZE_X);
    local_z = tc_math_floor_mod(z, TC_CHUNK_SIZE_Z);
    chunk = tc_world_chunk_at(world, chunk_x, chunk_z);

    if (chunk != NULL && tc_chunk_get(chunk, local_x, y, local_z) == block) {
        return 1;
    }

    if (!append_edit(world, x, y, z, block)) {
        return 0;
    }

    if (chunk != NULL) {
        apply_edit_to_loaded_chunk(world, chunk_x, chunk_z, x, y, z, block);
    }

    return 1;
}

void tc_world_clear_edits(tc_world *world) {
    if (world != NULL) {
        world->edit_count = 0;
    }
}

size_t tc_world_edit_count(const tc_world *world) {
    return world != NULL ? world->edit_count : 0;
}

const tc_world_edit *tc_world_edits(const tc_world *world) {
    return world != NULL ? world->edits : NULL;
}

void tc_world_set_seed(tc_world *world, uint32_t seed) {
    if (world == NULL) {
        return;
    }

    tc_world_clear_edits(world);
    world->seed = seed;

    if (world->generate_terrain) {
        for (int index = 0; index < world->size * world->size; index++) {
            tc_world_slot *slot = &world->slots[index];

            if (!slot->loaded) {
                continue;
            }

            tc_chunk_generate(&slot->chunk, world->seed);
            apply_edits_to_chunk(world, &slot->chunk);
            slot->dirty = 1;
        }
    }
}

void tc_world_generate_terrain(tc_world *world) {
    if (world == NULL || world->slots == NULL) {
        return;
    }

    world->generate_terrain = 1;

    for (int index = 0; index < world->size * world->size; index++) {
        tc_world_slot *slot = &world->slots[index];

        if (!slot->loaded) {
            continue;
        }

        tc_chunk_generate(&slot->chunk, world->seed);
        apply_edits_to_chunk(world, &slot->chunk);
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
