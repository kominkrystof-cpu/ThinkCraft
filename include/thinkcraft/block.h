#ifndef THINKCRAFT_BLOCK_H
#define THINKCRAFT_BLOCK_H

#include <stdint.h>

#define TC_BLOCK_AIR   0
#define TC_BLOCK_GRASS 1
#define TC_BLOCK_DIRT  2
#define TC_BLOCK_STONE 3
#define TC_BLOCK_SAND  4
#define TC_BLOCK_BEDROCK 5

#define TC_WORLD_BEDROCK_LAYERS 1
#define TC_WORLD_FLOOR_Y ((float)TC_WORLD_BEDROCK_LAYERS)

static inline int tc_block_is_solid(uint8_t block) {
    return block != TC_BLOCK_AIR;
}

static inline int tc_block_is_breakable(uint8_t block) {
    return block != TC_BLOCK_AIR && block != TC_BLOCK_BEDROCK;
}

#endif
