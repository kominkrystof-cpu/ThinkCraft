#ifndef THINKCRAFT_FALLING_H
#define THINKCRAFT_FALLING_H

#include "thinkcraft/world.h"

#include <stddef.h>
#include <stdint.h>

#define TC_FALLING_MAX_BLOCKS 1024
#define TC_FALLING_GRAVITY 30.0f
#define TC_FALLING_TERMINAL_VELOCITY 22.0f
#define TC_FALLING_MAX_STEP 0.02f

typedef struct {
    int32_t x;
    int32_t z;
    float y;
    float velocity;
    uint8_t block;
} tc_falling_block;

typedef struct {
    tc_falling_block blocks[TC_FALLING_MAX_BLOCKS];
    size_t count;
} tc_falling_blocks;

void tc_falling_init(tc_falling_blocks *falling);
size_t tc_falling_count(const tc_falling_blocks *falling);
const tc_falling_block *tc_falling_data(const tc_falling_blocks *falling);
int tc_falling_is_falling_block(uint8_t block);

/* Resolves unsupported sand after an edit, including cascades up the column.
 * Returns the number of blocks that started falling. */
int tc_falling_notify_block_changed(tc_falling_blocks *falling, tc_world *world,
                                    int x, int y, int z);

/* Advances every falling block. Returns the number of blocks that landed. */
int tc_falling_update(tc_falling_blocks *falling, tc_world *world, float dt);

/* Drops every in-flight block onto the first support below it. Used before a
 * save so mid-air sand is never lost with the transient entities. */
void tc_falling_settle_all(tc_falling_blocks *falling, tc_world *world);

#endif
