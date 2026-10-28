#include "thinkcraft/falling.h"
#include "thinkcraft/math.h"

#include <math.h>
#include <string.h>

#define TC_FALLING_EPSILON 0.0001f
#define TC_FALLING_INSERTION_SORT_MAX 24

typedef struct {
    tc_world *world;
    int x;
    int z;
    int local_x;
    int local_z;
    tc_chunk *chunk;
} tc_falling_column;

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

/* A falling block keeps a fixed x/z for its whole life. Resolve the chunk and
 * local coordinates once per block instead of doing a world lookup for every
 * cell probed while it falls. */
static void column_init(tc_falling_column *column, tc_world *world, int x,
                        int z) {
    int chunk_x = tc_math_floor_div(x, TC_CHUNK_SIZE_X);
    int chunk_z = tc_math_floor_div(z, TC_CHUNK_SIZE_Z);

    column->world = world;
    column->x = x;
    column->z = z;
    column->local_x = tc_math_floor_mod(x, TC_CHUNK_SIZE_X);
    column->local_z = tc_math_floor_mod(z, TC_CHUNK_SIZE_Z);
    column->chunk = tc_world_chunk_at(world, chunk_x, chunk_z);
}

static inline uint8_t column_block(const tc_falling_column *column, int y) {
    if (column->chunk == NULL || (unsigned)y >= TC_CHUNK_SIZE_Y) {
        return TC_BLOCK_AIR;
    }

    return column->chunk->blocks[
        tc_chunk_index(column->local_x, y, column->local_z)];
}

static inline int column_is_air(const tc_falling_column *column, int y) {
    return column_block(column, y) == TC_BLOCK_AIR;
}

static int place_block(const tc_falling_column *column, int y,
                       uint8_t block) {
    if (!column_is_air(column, y)) {
        return 0;
    }

    if (!tc_world_set_block(column->world, column->x, y, column->z, block)) {
        return 0;
    }

    /* The padded mesh cache reads across chunk borders, so both the chunk that
     * changed and any border neighbor must be rebuilt. */
    tc_world_mark_dirty_neighbors(column->world, column->x, y, column->z);
    return 1;
}

static int spawn_at_column(tc_falling_blocks *falling,
                           tc_falling_column *column, int y) {
    tc_falling_block *block;

    if (falling->count >= TC_FALLING_MAX_BLOCKS
        || y <= 0 || y >= TC_CHUNK_SIZE_Y) {
        return 0;
    }

    if (column_block(column, y) != TC_BLOCK_SAND
        || !column_is_air(column, y - 1)) {
        return 0;
    }

    if (!tc_world_set_block(column->world, column->x, y, column->z,
                            TC_BLOCK_AIR)) {
        return 0;
    }

    tc_world_mark_dirty_neighbors(column->world, column->x, y, column->z);

    block = &falling->blocks[falling->count++];
    block->x = column->x;
    block->z = column->z;
    block->y = (float)y;
    block->velocity = 0.0f;
    block->block = TC_BLOCK_SAND;
    return 1;
}

int tc_falling_notify_block_changed(tc_falling_blocks *falling, tc_world *world,
                                    int x, int y, int z) {
    tc_falling_column column;
    int spawned;

    if (falling == NULL || world == NULL) {
        return 0;
    }

    column_init(&column, world, x, z);
    spawned = spawn_at_column(falling, &column, y);

    /* If the edited cell is now empty, every sand block stacked above loses
     * its support in turn. Removing them top-down instead of one-per-tick
     * keeps a tall column from trickling. */
    if (column_is_air(&column, y)) {
        int cursor = y + 1;

        while (cursor < TC_CHUNK_SIZE_Y
               && spawn_at_column(falling, &column, cursor)) {
            spawned++;
            cursor++;
        }
    }

    return spawned;
}

static int height_cell(float y) {
    int cell = (int)floorf(y);

    if (cell < 0) {
        return 0;
    }
    if (cell >= TC_CHUNK_SIZE_Y) {
        return TC_CHUNK_SIZE_Y - 1;
    }
    return cell;
}

static void sort_by_height(tc_falling_blocks *falling) {
    if (falling->count < 2) {
        return;
    }

    if (falling->count <= TC_FALLING_INSERTION_SORT_MAX) {
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
        return;
    }

    /* y is bounded by the chunk height, so a stable counting sort is O(n) and
     * avoids the insertion sort's quadratic behavior during large collapses. */
    {
        tc_falling_block sorted[TC_FALLING_MAX_BLOCKS];
        size_t offsets[TC_CHUNK_SIZE_Y + 1] = {0};

        for (size_t index = 0; index < falling->count; index++) {
            offsets[height_cell(falling->blocks[index].y) + 1]++;
        }
        for (int cell = 1; cell <= TC_CHUNK_SIZE_Y; cell++) {
            offsets[cell] += offsets[cell - 1];
        }
        for (size_t index = 0; index < falling->count; index++) {
            int cell = height_cell(falling->blocks[index].y);

            sorted[offsets[cell]++] = falling->blocks[index];
        }

        memcpy(falling->blocks, sorted,
               falling->count * sizeof(falling->blocks[0]));
    }
}

static int step_block(tc_falling_block *block, tc_world *world, float dt,
                      int *landed_out) {
    tc_falling_column column;
    float remaining = dt;
    float y = block->y;
    float velocity = block->velocity;
    int landed = 0;

    *landed_out = 0;
    column_init(&column, world, block->x, block->z);

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
        if (!column_is_air(&column, current)) {
            int free_cell = current + 1;

            while (free_cell < TC_CHUNK_SIZE_Y
                   && !column_is_air(&column, free_cell)) {
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
            if (!column_is_air(&column, cell)) {
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
        if (place_block(&column, (int)y, block->block)) {
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
        tc_falling_column column;
        int cell = (int)floorf(block->y + TC_FALLING_EPSILON);
        int target;

        column_init(&column, world, block->x, block->z);

        if (cell < 0) {
            cell = 0;
        }

        target = 0;
        for (int below = cell - 1; below >= 0; below--) {
            if (!column_is_air(&column, below)) {
                target = below + 1;
                break;
            }
        }

        while (target < TC_CHUNK_SIZE_Y
               && !column_is_air(&column, target)) {
            target++;
        }

        if (target < TC_CHUNK_SIZE_Y) {
            place_block(&column, target, block->block);
        }
    }

    falling->count = 0;
}
