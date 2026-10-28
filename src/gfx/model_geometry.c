#include "thinkcraft/model_geometry.h"
#include "thinkcraft/block.h"
#include "thinkcraft/texture.h"

#include <stdint.h>

#define TC_MODEL_PIXEL 4
#define TC_MODEL_TOOL_DEPTH 4

static void set_vertex(tc_vertex *vertex, int x, int y, int z, int u, int v,
                       int shade, int tile) {
    vertex->position[0] = (uint16_t)x;
    vertex->position[1] = (uint16_t)y;
    vertex->position[2] = (uint16_t)z;
    vertex->uv[0] = (uint8_t)u;
    vertex->uv[1] = (uint8_t)v;
    vertex->shade = (uint8_t)shade;
    vertex->tile = (uint8_t)tile;
}

static int add_face_uv(tc_mesh *mesh, int x0, int y0, int z0,
                       int x1, int y1, int z1, int face, int shade,
                       int tile, int u0, int v0, int u1, int v1) {
    tc_vertex quad[4];

    switch (face) {
        case 0:
            set_vertex(&quad[0], x0, y1, z1, u0, v0, shade, tile);
            set_vertex(&quad[1], x1, y1, z1, u1, v0, shade, tile);
            set_vertex(&quad[2], x1, y1, z0, u1, v1, shade, tile);
            set_vertex(&quad[3], x0, y1, z0, u0, v1, shade, tile);
            break;
        case 1:
            set_vertex(&quad[0], x0, y0, z0, u0, v1, shade, tile);
            set_vertex(&quad[1], x1, y0, z0, u1, v1, shade, tile);
            set_vertex(&quad[2], x1, y0, z1, u1, v0, shade, tile);
            set_vertex(&quad[3], x0, y0, z1, u0, v0, shade, tile);
            break;
        case 2:
            set_vertex(&quad[0], x1, y0, z1, u0, v0, shade, tile);
            set_vertex(&quad[1], x1, y0, z0, u1, v0, shade, tile);
            set_vertex(&quad[2], x1, y1, z0, u1, v1, shade, tile);
            set_vertex(&quad[3], x1, y1, z1, u0, v1, shade, tile);
            break;
        case 3:
            set_vertex(&quad[0], x0, y0, z0, u0, v0, shade, tile);
            set_vertex(&quad[1], x0, y0, z1, u1, v0, shade, tile);
            set_vertex(&quad[2], x0, y1, z1, u1, v1, shade, tile);
            set_vertex(&quad[3], x0, y1, z0, u0, v1, shade, tile);
            break;
        case 4:
            set_vertex(&quad[0], x0, y0, z1, u0, v0, shade, tile);
            set_vertex(&quad[1], x1, y0, z1, u1, v0, shade, tile);
            set_vertex(&quad[2], x1, y1, z1, u1, v1, shade, tile);
            set_vertex(&quad[3], x0, y1, z1, u0, v1, shade, tile);
            break;
        case 5:
        default:
            set_vertex(&quad[0], x1, y0, z0, u0, v0, shade, tile);
            set_vertex(&quad[1], x0, y0, z0, u1, v0, shade, tile);
            set_vertex(&quad[2], x0, y1, z0, u1, v1, shade, tile);
            set_vertex(&quad[3], x1, y1, z0, u0, v1, shade, tile);
            break;
    }

    return tc_mesh_add_quad(mesh, quad);
}

static int add_face(tc_mesh *mesh, int x0, int y0, int z0,
                    int x1, int y1, int z1, int face, int shade, int tile) {
    return add_face_uv(mesh, x0, y0, z0, x1, y1, z1, face, shade, tile,
                       0, 0, TC_ATLAS_TILE_SIZE, TC_ATLAS_TILE_SIZE);
}

int tc_model_add_box(tc_mesh *mesh, int x0, int y0, int z0,
                     int width, int height, int depth, int top_tile,
                     int bottom_tile, int east_tile, int west_tile,
                     int south_tile, int north_tile) {
    int x1 = x0 + width;
    int y1 = y0 + height;
    int z1 = z0 + depth;

    return add_face(mesh, x0, y0, z0, x1, y1, z1, 0, 255, top_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 1, 140, bottom_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 2, 212, east_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 3, 190, west_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 4, 202, south_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 5, 174, north_tile);
}

int tc_model_add_uniform_box(tc_mesh *mesh, int x0, int y0, int z0,
                             int width, int height, int depth, int tile) {
    return tc_model_add_box(mesh, x0, y0, z0, width, height, depth, tile, tile,
                            tile, tile, tile, tile);
}

int tc_model_build_block(tc_mesh *mesh, uint8_t block) {
    int top;
    int side;
    int bottom;

    if (mesh == NULL) {
        return 0;
    }

    switch (block) {
        case TC_BLOCK_GRASS:
            top = TC_TILE_GRASS_TOP;
            side = TC_TILE_GRASS_SIDE;
            bottom = TC_TILE_DIRT;
            break;
        case TC_BLOCK_DIRT:
            top = side = bottom = TC_TILE_DIRT;
            break;
        case TC_BLOCK_STONE:
            top = side = bottom = TC_TILE_STONE;
            break;
        case TC_BLOCK_SAND:
            top = side = bottom = TC_TILE_SAND;
            break;
        case TC_BLOCK_BEDROCK:
            top = side = bottom = TC_TILE_BEDROCK;
            break;
        default:
            return 1;
    }

    return tc_model_add_box(mesh, 0, 0, 0, 64, 64, 64, top, bottom, side,
                            side, side, side);
}

static int pattern_filled(const char *pattern, int row, int column) {
    return pattern[row * (TC_ATLAS_TILE_SIZE + 1) + column] != '.';
}

int tc_model_build_tool(tc_mesh *mesh, uint8_t tile) {
    const char *pattern = tc_texture_tool_pattern(tile);

    if (mesh == NULL) {
        return 0;
    }
    if (pattern == NULL) {
        return 1;
    }

    for (int row = 0; row < TC_ATLAS_TILE_SIZE; row++) {
        for (int column = 0; column < TC_ATLAS_TILE_SIZE; column++) {
            int x0;
            int y0;
            int u0;
            int v0;
            int has_top;
            int has_bottom;
            int has_east;
            int has_west;

            if (!pattern_filled(pattern, row, column)) {
                continue;
            }

            x0 = column * TC_MODEL_PIXEL;
            y0 = (TC_ATLAS_TILE_SIZE - 1 - row) * TC_MODEL_PIXEL;
            u0 = column;
            v0 = TC_ATLAS_TILE_SIZE - 1 - row;

            has_top = row == 0 || !pattern_filled(pattern, row - 1, column);
            has_bottom = row == TC_ATLAS_TILE_SIZE - 1
                      || !pattern_filled(pattern, row + 1, column);
            has_east = column == TC_ATLAS_TILE_SIZE - 1
                    || !pattern_filled(pattern, row, column + 1);
            has_west = column == 0
                    || !pattern_filled(pattern, row, column - 1);

            if (has_top
                && !add_face_uv(mesh, x0, y0, 0,
                                x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                                TC_MODEL_TOOL_DEPTH, 0, 235, tile,
                                u0, v0, u0 + 1, v0 + 1)) {
                return 0;
            }
            if (has_bottom
                && !add_face_uv(mesh, x0, y0, 0,
                                x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                                TC_MODEL_TOOL_DEPTH, 1, 150, tile,
                                u0, v0, u0 + 1, v0 + 1)) {
                return 0;
            }
            if (has_east
                && !add_face_uv(mesh, x0, y0, 0,
                                x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                                TC_MODEL_TOOL_DEPTH, 2, 210, tile,
                                u0, v0, u0 + 1, v0 + 1)) {
                return 0;
            }
            if (has_west
                && !add_face_uv(mesh, x0, y0, 0,
                                x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                                TC_MODEL_TOOL_DEPTH, 3, 190, tile,
                                u0, v0, u0 + 1, v0 + 1)) {
                return 0;
            }
            if (!add_face_uv(mesh, x0, y0, 0,
                             x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                             TC_MODEL_TOOL_DEPTH, 4, 255, tile,
                             u0, v0, u0 + 1, v0 + 1)
                || !add_face_uv(mesh, x0, y0, 0,
                                x0 + TC_MODEL_PIXEL, y0 + TC_MODEL_PIXEL,
                                TC_MODEL_TOOL_DEPTH, 5, 220, tile,
                                u0, v0, u0 + 1, v0 + 1)) {
                return 0;
            }
        }
    }

    return 1;
}
