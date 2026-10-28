#include "thinkcraft/falling.h"

#include <math.h>
#include <string.h>

#define TC_FALLING_EPSILON 0.0001f

void tc_falling_init(tc_falling_blocks *falling) {
    if (falling != NULL) {
        falling->count = 0;
    }
}

size_t tc_falling_count(const tc_falling_blocks *falling) {
    return falling != NULL ? falling->count : 0;
}

const tc_falling_block *tc_falling_data(const tc_falling_blocks *falling) {
    return falling != NULL ? falling->blocks : NULL;
}

int tc_falling_is_falling_block(uint8_t block) {
    return block == TC_BLOCK_SAND;
}

static int is_air(const tc_world *world, int x, int y, int z) {
    return tc_world_get_block(world, x, y, z) == TC_BLOCK_AIR;
}

static int spawn_at(tc_falling_blocks *falling, tc_world *world, int x, int y,
                    int z) {
    tc_falling_block *block;

    if (falling->count >= TC_FALLING_MAX_BLOCKS
        || y <= 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    if (tc_world_get_block(world, x, y, z) != TC_BLOCK_SAND) {
        return 0;
    }

    if (!is_air(world, x, y - 1, z)) {
        return 0;
    }

    if (!tc_world_set_block(world, x, y, z, TC_BLOCK_AIR)) {
        return 0;
    }

    block = &falling->blocks[falling->count++];
    block->x = x;
    block->z = z;
    block->y = (float)y;
    block->velocity = 0.0f;
    block->block = TC_BLOCK_SAND;
    return 1;
}

int tc_falling_notify_block_changed(tc_falling_blocks *falling, tc_world *world,
                                    int x, int y, int z) {
    int spawned;

    if (falling == NULL || world == NULL) {
        return 0;
    }

    spawned = spawn_at(falling, world, x, y, z);

    /* If the edited cell is now empty, every sand block stacked above loses
     * its support in turn. Removing them top-down instead of one-per-tick
     * keeps a tall column from trickling. */
    if (is_air(world, x, y, z)) {
        int cursor = y + 1;

        while (cursor < TC_CHUNK_SIZE_Y
               && spawn_at(falling, world, x, cursor, z)) {
            spawned++;
            cursor++;
        }
    }

    return spawned;
}

static int place_block(tc_world *world, int x, int y, int z, uint8_t block) {
    if (y < 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    if (!is_air(world, x, y, z)) {
        return 0;
    }

    return tc_world_set_block(world, x, y, z, block);
}

static void sort_by_height(tc_falling_blocks *falling) {
    for (size_t index = 1; index < falling->count; index++) {
        tc_falling_block item = falling->blocks[index];
        size_t position = index;

        while (position > 0
               && item.y < falling->blocks[position - 1].y) {
            falling->blocks[position] = falling->blocks[position - 1];
            position--;
        }
        falling->blocks[position] = item;
    }
}

static int step_block(tc_falling_block *block, tc_world *world, float dt,
                      int *landed_out) {
    float remaining = dt;
    float y = block->y;
    float velocity = block->velocity;
    int landed = 0;

    *landed_out = 0;

    while (remaining > 0.0f && !landed) {
        float step = remaining > TC_FALLING_MAX_STEP ? TC_FALLING_MAX_STEP
                                                     : remaining;
        float next;
        int current;
        int next_cell;
        int hit = 0;
        int land_cell;

        remaining -= step;
        velocity -= TC_FALLING_GRAVITY * step;
        if (velocity < -TC_FALLING_TERMINAL_VELOCITY) {
            velocity = -TC_FALLING_TERMINAL_VELOCITY;
        }

        current = (int)floorf(y + TC_FALLING_EPSILON);

        /* A player can place a block inside a falling block. Push the falling
         * block back out to the first free cell instead of letting it tunnel. */
        if (!is_air(world, block->x, current, block->z)) {
            int free_cell = current + 1;

            while (free_cell < TC_CHUNK_SIZE_Y
                   && !is_air(world, block->x, free_cell, block->z)) {
                free_cell++;
            }

            if (free_cell >= TC_CHUNK_SIZE_Y) {
                *landed_out = 1;
                return 1;
            }

            y = (float)free_cell;
            velocity = 0.0f;
            block->velocity = 0.0f;
            block->y = y;
            return 1;
        }

        next = y + velocity * step;
        next_cell = (int)floorf(next + TC_FALLING_EPSILON);
        if (next_cell < 0) {
            next_cell = 0;
        }

        land_cell = current;
        for (int cell = current - 1; cell >= next_cell; cell--) {
            if (!is_air(world, block->x, cell, block->z)) {
                land_cell = cell + 1;
                hit = 1;
                break;
            }
        }

        if (!hit && next < 0.0f) {
            land_cell = 0;
            hit = 1;
        }

        if (hit) {
            y = (float)land_cell;
            landed = 1;
        } else {
            y = next;
        }
    }

    block->velocity = landed ? 0.0f : velocity;

    if (landed) {
        *landed_out = 1;
        if (place_block(world, block->x, (int)y, block->z, block->block)) {
            return 1;
        }
        return 0;
    }

    block->y = y;
    return 1;
}

int tc_falling_update(tc_falling_blocks *falling, tc_world *world, float dt) {
    size_t read_index = 0;
    size_t write_index = 0;
    int landed = 0;

    if (falling == NULL || world == NULL || dt <= 0.0f) {
        return 0;
    }

    /* Processing bottom-up guarantees a lower block settles before the block
     * above it, so stacked columns cannot try to occupy the same cell. */
    sort_by_height(falling);

    /* Fixed substeps keep fast falls from skipping through thin floors. */
    while (read_index < falling->count) {
        tc_falling_block block = falling->blocks[read_index++];
        int block_landed = 0;

        if (!step_block(&block, world, dt, &block_landed)) {
            continue;
        }

        if (block_landed) {
            landed++;
            continue;
        }

        falling->blocks[write_index++] = block;
    }

    falling->count = write_index;
    return landed;
}

void tc_falling_settle_all(tc_falling_blocks *falling, tc_world *world) {
    if (falling == NULL || world == NULL) {
        return;
    }

    sort_by_height(falling);

    for (size_t index = 0; index < falling->count; index++) {
        const tc_falling_block *block = &falling->blocks[index];
        int cell = (int)floorf(block->y + TC_FALLING_EPSILON);
        int target;

        if (cell < 0) {
            cell = 0;
        }

        target = 0;
        for (int below = cell - 1; below >= 0; below--) {
            if (!is_air(world, block->x, below, block->z)) {
                target = below + 1;
                break;
            }
        }

        while (target < TC_CHUNK_SIZE_Y
               && !is_air(world, block->x, target, block->z)) {
            target++;
        }

        if (target < TC_CHUNK_SIZE_Y) {
            place_block(world, block->x, target, block->z, block->block);
        }
    }

    falling->count = 0;
}
