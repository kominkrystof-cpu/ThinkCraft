#ifndef THINKCRAFT_MODEL_GEOMETRY_H
#define THINKCRAFT_MODEL_GEOMETRY_H

#include "thinkcraft/mesh.h"

#include <stdint.h>

#define TC_MODEL_UNIT (1.0f / 64.0f)

/* Builds closed, six-faced 64-unit block geometry. */
int tc_model_build_block(tc_mesh *mesh, uint8_t block);

/* Extrudes a 16x16 tool silhouette into a closed pixel-art model. */
int tc_model_build_tool(tc_mesh *mesh, uint8_t tile);

int tc_model_add_box(tc_mesh *mesh, int x0, int y0, int z0,
                     int width, int height, int depth, int top_tile,
                     int bottom_tile, int east_tile, int west_tile,
                     int south_tile, int north_tile);
int tc_model_add_uniform_box(tc_mesh *mesh, int x0, int y0, int z0,
                             int width, int height, int depth, int tile);

#endif
