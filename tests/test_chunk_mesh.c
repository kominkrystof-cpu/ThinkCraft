#include "thinkcraft/chunk.h"
#include "thinkcraft/chunk_mesh.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/terrain.h"
#include "thinkcraft/texture.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_chunk_mesh: %s\n", message);
    }
    return condition;
}

static void fill_box(tc_chunk *chunk, int x0, int y0, int z0,
                     int x1, int y1, int z1, uint8_t block) {
    for (int y = y0; y <= y1; y++) {
        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                tc_chunk_set(chunk, x, y, z, block);
            }
        }
    }
}

static int validate_mesh(const tc_mesh *mesh, size_t expected_quads) {
    if (!check(mesh->quad_count == expected_quads, "wrong quad count")
        || !check(mesh->vertex_count == expected_quads * 4,
                  "wrong vertex count")
        || !check(mesh->index_count == expected_quads * 6,
                  "wrong index count")) {
        return 0;
    }

    for (size_t i = 0; i < mesh->index_count; i++) {
        if (!check(mesh->indices[i] < mesh->vertex_count,
                   "index out of range")) {
            return 0;
        }
    }

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        const tc_vertex *vertex = &mesh->vertices[i];
        if (!check(vertex->position[0] >= 0.0f
                   && vertex->position[0] <= (float)TC_CHUNK_SIZE_X,
                   "x position out of range")
            || !check(vertex->position[1] >= 0.0f
                      && vertex->position[1] <= (float)TC_CHUNK_SIZE_Y,
                      "y position out of range")
            || !check(vertex->position[2] >= 0.0f
                      && vertex->position[2] <= (float)TC_CHUNK_SIZE_Z,
                      "z position out of range")
            || !check(vertex->tile < TC_TILE_COUNT, "invalid vertex tile")) {
            return 0;
        }
    }

    return 1;
}

static int build_and_check(tc_chunk *chunk, size_t expected_quads) {
    tc_mesh mesh;
    tc_mesh_stats stats;
    int ok;

    tc_mesh_init(&mesh);
    ok = check(tc_chunk_build_mesh(chunk, &mesh, &stats),
               "mesh build failed")
      && validate_mesh(&mesh, expected_quads);
    tc_mesh_shutdown(&mesh);
    return ok;
}

static int test_chunk_access(void) {
    tc_chunk *chunk = malloc(sizeof(*chunk));
    int ok;

    if (chunk == NULL) {
        return 0;
    }

    tc_chunk_init(chunk, 1, 2, 3);

    ok = check(chunk->origin_x == 1 && chunk->origin_y == 2
               && chunk->origin_z == 3, "wrong chunk origin")
      && check(tc_chunk_set(chunk, 3, 4, 5, TC_BLOCK_STONE),
               "chunk set failed")
      && check(tc_chunk_get(chunk, 3, 4, 5) == TC_BLOCK_STONE,
               "chunk get failed")
      && check(chunk->solid_count == 1, "solid count increment failed")
      && check(tc_chunk_count_solid(chunk->blocks, TC_CHUNK_BLOCK_COUNT) == 1,
               "asm solid count failed")
      && check(tc_chunk_set(chunk, 3, 4, 5, TC_BLOCK_AIR),
               "chunk clear block failed")
      && check(chunk->solid_count == 0, "solid count decrement failed")
      && check(tc_chunk_count_solid(chunk->blocks, TC_CHUNK_BLOCK_COUNT) == 0,
               "asm solid recount failed")
      && check(tc_chunk_set(chunk, -1, 0, 0, TC_BLOCK_STONE) == 0,
               "out-of-bounds set accepted")
      && check(tc_chunk_get(chunk, -1, 0, 0) == TC_BLOCK_AIR,
               "out-of-bounds get was not air");

    free(chunk);
    return ok;
}

static int test_greedy_cases(void) {
    tc_chunk *chunk = malloc(sizeof(*chunk));
    int ok = 1;

    if (chunk == NULL) {
        return 0;
    }

    tc_chunk_init(chunk, 0, 0, 0);
    ok = ok && check(tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_STONE),
                     "single block set failed");
    ok = ok && build_and_check(chunk, 6);

    tc_chunk_clear(chunk);
    fill_box(chunk, 4, 4, 4, 6, 4, 4, TC_BLOCK_STONE);
    ok = ok && build_and_check(chunk, 6);

    tc_chunk_clear(chunk);
    fill_box(chunk, 4, 4, 4, 5, 5, 5, TC_BLOCK_STONE);
    ok = ok && build_and_check(chunk, 6);

    tc_chunk_clear(chunk);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_STONE);
    tc_chunk_set(chunk, 8, 4, 8, TC_BLOCK_STONE);
    ok = ok && build_and_check(chunk, 12);

    tc_chunk_clear(chunk);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_STONE);
    tc_chunk_set(chunk, 5, 4, 4, TC_BLOCK_DIRT);
    ok = ok && build_and_check(chunk, 10);

    free(chunk);
    return ok;
}

static int test_face_tiles(void) {
    tc_chunk *chunk = malloc(sizeof(*chunk));
    tc_mesh mesh;
    tc_mesh_stats stats;
    int counts[TC_TILE_COUNT] = {0};
    int ok = 1;

    if (chunk == NULL) {
        return 0;
    }

    tc_chunk_init(chunk, 0, 0, 0);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_GRASS);
    tc_mesh_init(&mesh);
    ok = ok && check(tc_chunk_build_mesh(chunk, &mesh, &stats),
                     "grass tile mesh build failed");
    ok = ok && check(mesh.quad_count == 6, "grass block did not produce 6 quads");

    for (size_t quad = 0; quad < mesh.quad_count; quad++) {
        uint8_t tile = mesh.vertices[quad * 4].tile;

        for (int vertex = 1; vertex < 4; vertex++) {
            if (!check(mesh.vertices[quad * 4 + vertex].tile == tile,
                       "quad uses multiple tiles")) {
                ok = 0;
            }
        }
        if (tile >= TC_TILE_COUNT) {
            ok = check(0, "quad tile out of range");
        } else {
            counts[tile]++;
        }
    }

    ok = ok && check(counts[TC_TILE_GRASS_TOP] == 1,
                     "grass top tile missing")
      && check(counts[TC_TILE_DIRT] == 1,
               "grass bottom did not use dirt")
      && check(counts[TC_TILE_GRASS_SIDE] == 4,
               "grass side tiles missing");

    tc_chunk_clear(chunk);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_SAND);
    ok = ok && check(tc_chunk_build_mesh(chunk, &mesh, &stats),
                     "sand tile mesh build failed");
    for (size_t quad = 0; quad < mesh.quad_count; quad++) {
        if (!check(mesh.vertices[quad * 4].tile == TC_TILE_SAND,
                   "sand did not use the sand tile")) {
            ok = 0;
        }
    }

    tc_chunk_clear(chunk);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_STONE);
    ok = ok && check(tc_chunk_build_mesh(chunk, &mesh, &stats),
                     "stone tile mesh build failed");
    for (size_t quad = 0; quad < mesh.quad_count; quad++) {
        if (!check(mesh.vertices[quad * 4].tile == TC_TILE_STONE,
                   "stone did not use the stone tile")) {
            ok = 0;
        }
    }

    tc_chunk_clear(chunk);
    tc_chunk_set(chunk, 4, 4, 4, TC_BLOCK_BEDROCK);
    ok = ok && check(tc_chunk_build_mesh(chunk, &mesh, &stats),
                     "bedrock tile mesh build failed");
    for (size_t quad = 0; quad < mesh.quad_count; quad++) {
        if (!check(mesh.vertices[quad * 4].tile == TC_TILE_BEDROCK,
                   "bedrock did not use the bedrock tile")) {
            ok = 0;
        }
    }

    if (ok) {
        printf("face tiles: grass top/side/bottom, sand, stone, and bedrock "
               "mapping passed\n");
    }

    tc_mesh_shutdown(&mesh);
    free(chunk);
    return ok;
}

static int test_generated_mesh(void) {
    tc_chunk *chunk = malloc(sizeof(*chunk));
    tc_mesh mesh;
    tc_mesh_stats stats;
    int ok = 1;

    if (chunk == NULL) {
        return 0;
    }

    tc_chunk_init(chunk, 0, 0, 0);
    tc_chunk_generate(chunk, TC_TERRAIN_DEFAULT_SEED);

    tc_mesh_init(&mesh);
    ok = ok && check(tc_chunk_build_mesh(chunk, &mesh, &stats),
                     "generated mesh build failed");
    ok = ok && check(stats.solid_blocks == chunk->solid_count,
                     "generated solid count mismatch");
    ok = ok && check(stats.solid_blocks
                         == tc_chunk_count_solid(chunk->blocks,
                                                 TC_CHUNK_BLOCK_COUNT),
                     "generated asm solid count mismatch");
    ok = ok && check(stats.quads > 0, "generated mesh is empty");
    ok = ok && check(stats.quads < stats.visible_faces,
                     "greedy meshing did not reduce faces");
    ok = ok && check(mesh.quad_count == stats.quads,
                     "generated stats quad mismatch after build");
    ok = ok && validate_mesh(&mesh, stats.quads);

    printf("generated chunk: %zu solid blocks, %zu faces -> %zu quads "
           "(%.2fx reduction), %.3f ms build\n",
           stats.solid_blocks, stats.visible_faces, stats.quads,
           stats.quads > 0
               ? (double)stats.visible_faces / (double)stats.quads
               : 0.0,
           stats.build_ms);

    tc_mesh_shutdown(&mesh);
    free(chunk);
    return ok;
}

static int test_mesh_performance(void) {
    const int iterations = 32;
    tc_chunk *chunk = malloc(sizeof(*chunk));
    tc_mesh mesh;
    double total_build_ms = 0.0;
    double total_scan_ms = 0.0;
    int ok = 1;

    if (chunk == NULL) {
        return 0;
    }

    tc_chunk_init(chunk, 0, 0, 0);
    tc_chunk_generate(chunk, TC_TERRAIN_DEFAULT_SEED);
    tc_mesh_init(&mesh);

    for (int i = 0; i < iterations; i++) {
        tc_mesh_stats stats;
        if (!tc_chunk_build_mesh(chunk, &mesh, &stats)) {
            ok = 0;
            break;
        }
        total_build_ms += stats.build_ms;
        total_scan_ms += stats.scan_ms;
    }

    if (ok) {
        printf("mesh performance: %.3f ms mesh build average, "
               "%.3f ms visibility scan average over %d builds\n",
               total_build_ms / (double)iterations,
               total_scan_ms / (double)iterations,
               iterations);
    }

    tc_mesh_shutdown(&mesh);
    free(chunk);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_chunk_access();
    ok = ok && test_greedy_cases();
    ok = ok && test_face_tiles();
    ok = ok && test_generated_mesh();
    ok = ok && test_mesh_performance();

    if (!ok) {
        return 1;
    }

    printf("test_chunk_mesh: all checks passed\n");
    return 0;
}
