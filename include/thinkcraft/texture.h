#ifndef THINKCRAFT_TEXTURE_H
#define THINKCRAFT_TEXTURE_H

#include <stdint.h>

#define TC_ATLAS_TILE_SIZE 16
#define TC_ATLAS_GRID 4
#define TC_ATLAS_SIZE (TC_ATLAS_TILE_SIZE * TC_ATLAS_GRID)
#define TC_ATLAS_PIXEL_COUNT (TC_ATLAS_SIZE * TC_ATLAS_SIZE * 4)

typedef enum {
    TC_TILE_GRASS_TOP,
    TC_TILE_GRASS_SIDE,
    TC_TILE_DIRT,
    TC_TILE_STONE,
    TC_TILE_SAND,
    TC_TILE_COUNT
} tc_atlas_tile;

void tc_texture_generate_atlas(uint8_t *pixels);

#endif
