#ifndef THINKCRAFT_TERRAIN_H
#define THINKCRAFT_TERRAIN_H

#include "thinkcraft/chunk.h"

#include <stdint.h>

#define TC_TERRAIN_DEFAULT_SEED 0x5eed1234u

int tc_terrain_height(int x, int z, uint32_t seed);
void tc_terrain_generate_chunk(tc_chunk *chunk, uint32_t seed);

#endif
