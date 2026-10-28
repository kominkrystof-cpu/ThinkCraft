#include "thinkcraft/block.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/model_geometry.h"
#include "thinkcraft/texture.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_model: %s\n", message);
    }
    return condition;
}

static int quad_normal(const tc_mesh *mesh, size_t quad, int normal[3]) {
    const tc_vertex *v = &mesh->vertices[quad * 4];
    int ax = (int)v[1].position[0] - (int)v[0].position[0];
    int ay = (int)v[1].position[1] - (int)v[0].position[1];
    int az = (int)v[1].position[2] - (int)v[0].position[2];
    int bx = (int)v[2].position[0] - (int)v[0].position[0];
    int by = (int)v[2].position[1] - (int)v[0].position[1];
    int bz = (int)v[2].position[2] - (int)v[0].position[2];
    int nx = ay * bz - az * by;
    int ny = az * bx - ax * bz;
    int nz = ax * by - ay * bx;
    int magnitude = abs(nx) + abs(ny) + abs(nz);

    if (magnitude == 0) {
        return 0;
    }

    normal[0] = nx / magnitude;
    normal[1] = ny / magnitude;
    normal[2] = nz / magnitude;
    return 1;
}

static int test_block_faces(void) {
    static const int expected_faces[6][3] = {
        {0, 1, 0}, {0, -1, 0}, {1, 0, 0},
        {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };
    tc_mesh mesh;
    int ok = 1;

    for (uint8_t block = TC_BLOCK_GRASS; block <= TC_BLOCK_BEDROCK; block++) {
        int seen[6] = {0};

        tc_mesh_init(&mesh);
        ok = ok && check(tc_model_build_block(&mesh, block),
                         "block mesh build failed")
           && check(mesh.quad_count == 6, "block does not have six faces")
           && check(mesh.vertex_count == 24, "block vertex count is wrong")
           && check(mesh.index_count == 36, "block index count is wrong");

        for (size_t quad = 0; quad < mesh.quad_count; quad++) {
            int normal[3];
            int matched = 0;

            if (!check(quad_normal(&mesh, quad, normal),
                       "block face has degenerate winding")) {
                ok = 0;
                continue;
            }

            for (int face = 0; face < 6; face++) {
                if (normal[0] == expected_faces[face][0]
                    && normal[1] == expected_faces[face][1]
                    && normal[2] == expected_faces[face][2]) {
                    seen[face]++;
                    matched = 1;
                }
            }
            ok = ok && check(matched, "block face normal is not axis aligned");
        }

        for (int face = 0; face < 6; face++) {
            ok = ok && check(seen[face] == 1,
                             "block is missing a face or has a duplicate");
        }

        for (size_t vertex = 0; vertex < mesh.vertex_count; vertex++) {
            const tc_vertex *v = &mesh.vertices[vertex];

            ok = ok && check(v->uv[0] <= TC_ATLAS_TILE_SIZE
                             && v->uv[1] <= TC_ATLAS_TILE_SIZE,
                             "block UV exceeds one atlas tile");
            if (block == TC_BLOCK_GRASS) {
                if (vertex < 4) {
                    ok = ok && check(v->tile == TC_TILE_GRASS_TOP,
                                     "grass top tile is wrong");
                } else if (vertex < 8) {
                    ok = ok && check(v->tile == TC_TILE_DIRT,
                                     "grass bottom tile is wrong");
                } else {
                    ok = ok && check(v->tile == TC_TILE_GRASS_SIDE,
                                     "grass side tile is wrong");
                }
            }
        }

        tc_mesh_shutdown(&mesh);
    }

    return ok;
}

static size_t tool_expected_quads(const char *pattern) {
    size_t quads = 0;

    for (int row = 0; row < TC_ATLAS_TILE_SIZE; row++) {
        for (int column = 0; column < TC_ATLAS_TILE_SIZE; column++) {
            const char *line = pattern + row * (TC_ATLAS_TILE_SIZE + 1);

            if (line[column] == '.') {
                continue;
            }

            quads += 2;
            if (row == 0
                || pattern[(row - 1) * (TC_ATLAS_TILE_SIZE + 1) + column]
                   == '.') {
                quads++;
            }
            if (row == TC_ATLAS_TILE_SIZE - 1
                || pattern[(row + 1) * (TC_ATLAS_TILE_SIZE + 1) + column]
                   == '.') {
                quads++;
            }
            if (column == 0 || line[column - 1] == '.') {
                quads++;
            }
            if (column == TC_ATLAS_TILE_SIZE - 1 || line[column + 1] == '.') {
                quads++;
            }
        }
    }

    return quads;
}

static int test_tool_geometry(void) {
    static const uint8_t tools[] = {
        TC_TILE_WOODEN_PICKAXE, TC_TILE_WOODEN_AXE, TC_TILE_WOODEN_SHOVEL,
        TC_TILE_STONE_PICKAXE, TC_TILE_STONE_AXE, TC_TILE_STONE_SHOVEL
    };
    int ok = 1;

    for (size_t index = 0; index < sizeof(tools) / sizeof(tools[0]); index++) {
        const char *pattern = tc_texture_tool_pattern(tools[index]);
        tc_mesh mesh;

        ok = ok && check(pattern != NULL, "tool pattern is missing");
        if (pattern == NULL) {
            continue;
        }

        for (int row = 0; row < TC_ATLAS_TILE_SIZE; row++) {
            const char *line = pattern + row * (TC_ATLAS_TILE_SIZE + 1);

            ok = ok && check(line[TC_ATLAS_TILE_SIZE] == '\0',
                             "tool pattern row is not terminated");
            for (int column = 0; column < TC_ATLAS_TILE_SIZE; column++) {
                char pixel = line[column];

                ok = ok && check(pixel == '.' || pixel == 'M'
                                 || pixel == 'L' || pixel == 'H',
                                 "tool pattern has an invalid pixel");
            }
        }

        tc_mesh_init(&mesh);
        ok = ok && check(tc_model_build_tool(&mesh, tools[index]),
                         "tool mesh build failed")
           && check(mesh.quad_count == tool_expected_quads(pattern),
                    "tool quad count is wrong")
           && check(mesh.quad_count > 6, "tool geometry is not extruded");

        for (size_t vertex = 0; vertex < mesh.vertex_count; vertex++) {
            const tc_vertex *v = &mesh.vertices[vertex];

            ok = ok && check(v->position[0] <= 64 && v->position[1] <= 64,
                             "tool position is outside its 64-unit sprite")
               && check(v->position[2] <= 4,
                        "tool thickness exceeds one pixel")
               && check(v->uv[0] <= TC_ATLAS_TILE_SIZE
                        && v->uv[1] <= TC_ATLAS_TILE_SIZE,
                        "tool UV exceeds one atlas tile")
               && check(v->tile == tools[index], "tool tile is wrong");
        }

        for (size_t quad = 0; quad < mesh.quad_count; quad++) {
            int normal[3];

            ok = ok && check(quad_normal(&mesh, quad, normal),
                             "tool face has degenerate winding")
               && check(abs(normal[0]) + abs(normal[1]) + abs(normal[2]) == 1,
                        "tool face normal is not axis aligned");
        }

        tc_mesh_shutdown(&mesh);
    }

    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_block_faces();
    ok = ok && test_tool_geometry();

    if (!ok) {
        return 1;
    }

    printf("test_model: all checks passed\n");
    return 0;
}
