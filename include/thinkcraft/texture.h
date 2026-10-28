#ifndef THINKCRAFT_TEXTURE_H
#define THINKCRAFT_TEXTURE_H

#include <stdint.h>

#define TC_ATLAS_TILE_SIZE 16
#define TC_ATLAS_GRID 8
#define TC_ATLAS_SIZE (TC_ATLAS_TILE_SIZE * TC_ATLAS_GRID)
#define TC_ATLAS_PIXEL_COUNT (TC_ATLAS_SIZE * TC_ATLAS_SIZE * 4)

typedef enum {
    TC_TILE_GRASS_TOP,
    TC_TILE_GRASS_SIDE,
    TC_TILE_DIRT,
    TC_TILE_STONE,
    TC_TILE_SAND,
    TC_TILE_BEDROCK,
    TC_TILE_WOODEN_PICKAXE,
    TC_TILE_WOODEN_AXE,
    TC_TILE_WOODEN_SHOVEL,
    TC_TILE_STONE_PICKAXE,
    TC_TILE_STONE_AXE,
    TC_TILE_STONE_SHOVEL,
    TC_TILE_SKIN,
    TC_TILE_SHIRT,
    TC_TILE_PANTS,
    TC_TILE_HAIR,
    TC_TILE_FACE,
    TC_TILE_COUNT
} tc_atlas_tile;

void tc_texture_generate_atlas(uint8_t *pixels);

/* Returns a 16x16 tool silhouette as 16 rows of 17 bytes (16 characters plus
 * a NUL terminator), or NULL when the tile is not a tool. Row 0 is the top. */
const char *tc_texture_tool_pattern(uint8_t tile);

#endif
