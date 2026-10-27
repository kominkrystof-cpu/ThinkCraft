#include "thinkcraft/chunk_mesh.h"
#include "thinkcraft/platform.h"
#include "thinkcraft/texture.h"

#include <stdint.h>
#include <string.h>

#define TC_CHUNK_MASK_CAPACITY \
    (TC_CHUNK_SIZE_Y * (TC_CHUNK_SIZE_X > TC_CHUNK_SIZE_Z \
        ? TC_CHUNK_SIZE_X : TC_CHUNK_SIZE_Z))

static uint8_t to_byte(float value) {
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }
    return (uint8_t)(value * 255.0f + 0.5f);
}

static uint8_t sample_block(const tc_chunk *chunk, tc_block_sampler sampler,
                            const void *context, int x, int y, int z) {
    if (sampler != NULL) {
        return sampler(context, x, y, z);
    }
    return tc_chunk_get(chunk, x, y, z);
}

static uint8_t block_face_tile(uint8_t block, int axis, int positive) {
    if (block == TC_BLOCK_GRASS) {
        if (axis == 1) {
            return positive ? TC_TILE_GRASS_TOP : TC_TILE_DIRT;
        }
        return TC_TILE_GRASS_SIDE;
    }
    if (block == TC_BLOCK_DIRT) {
        return TC_TILE_DIRT;
    }
    if (block == TC_BLOCK_SAND) {
        return TC_TILE_SAND;
    }
    if (block == TC_BLOCK_BEDROCK) {
        return TC_TILE_BEDROCK;
    }
    return TC_TILE_STONE;
}

static void block_face_style(uint8_t block, int axis, int positive,
                             uint8_t *shade_out, uint8_t *tile) {
    float shade;

    if (axis == 1) {
        shade = positive ? 1.0f : 0.55f;
    } else if (axis == 0) {
        shade = positive ? 0.86f : 0.76f;
    } else {
        shade = positive ? 0.74f : 0.64f;
    }

    *shade_out = to_byte(shade);
    *tile = block_face_tile(block, axis, positive);
}

int tc_chunk_build_mesh_sampled(const tc_chunk *chunk,
                                tc_block_sampler sampler, const void *context,
                                tc_mesh *mesh, tc_mesh_stats *stats_out) {
    int32_t mask[TC_CHUNK_MASK_CAPACITY];
    tc_mesh_stats stats = {0};
    int dimensions[3] = {
        TC_CHUNK_SIZE_X,
        TC_CHUNK_SIZE_Y,
        TC_CHUNK_SIZE_Z
    };
    int coordinate[3] = {0, 0, 0};
    int max_y = -1;
    double scan_start;
    double build_start;

    if (chunk == NULL || mesh == NULL) {
        return 0;
    }

    tc_mesh_clear(mesh);
    mesh->origin[0] = (float)chunk->origin_x;
    mesh->origin[1] = (float)chunk->origin_y;
    mesh->origin[2] = (float)chunk->origin_z;

    scan_start = tc_time_now();
    stats.solid_blocks = tc_chunk_count_solid(chunk->blocks,
                                              TC_CHUNK_BLOCK_COUNT);
    stats.visible_faces = tc_chunk_visible_face_count_sampled(
        chunk, sampler, context);
    stats.scan_ms = (tc_time_now() - scan_start) * 1000.0;
    build_start = tc_time_now();

    for (int y = TC_CHUNK_SIZE_Y - 1; y >= 0; y--) {
        const uint8_t *layer = chunk->blocks
            + (size_t)y * TC_CHUNK_SIZE_X * TC_CHUNK_SIZE_Z;
        int found = 0;

        for (int i = 0; i < TC_CHUNK_SIZE_X * TC_CHUNK_SIZE_Z; i++) {
            if (layer[i] != TC_BLOCK_AIR) {
                found = 1;
                break;
            }
        }

        if (found) {
            max_y = y;
            break;
        }
    }

    if (max_y < 0) {
        stats.build_ms = (tc_time_now() - build_start) * 1000.0;
        if (stats_out != NULL) {
            *stats_out = stats;
        }
        return 1;
    }

    dimensions[1] = max_y + 1;

    for (int axis = 0; axis < 3; axis++) {
        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;
        int mask_width = dimensions[u];
        int mask_height = dimensions[v];

        for (coordinate[axis] = -1;
             coordinate[axis] < dimensions[axis];
             coordinate[axis]++) {
            int n = 0;

            for (coordinate[v] = 0; coordinate[v] < mask_height;
                 coordinate[v]++) {
                for (coordinate[u] = 0; coordinate[u] < mask_width;
                     coordinate[u]++, n++) {
                    int neighbor_coordinate[3] = {
                        coordinate[0],
                        coordinate[1],
                        coordinate[2]
                    };
                    uint8_t a;
                    uint8_t b;
                    int a_solid;
                    int b_solid;

                    neighbor_coordinate[axis]++;

                    a = sample_block(chunk, sampler, context,
                                     coordinate[0], coordinate[1],
                                     coordinate[2]);
                    b = sample_block(chunk, sampler, context,
                                     neighbor_coordinate[0],
                                     neighbor_coordinate[1],
                                     neighbor_coordinate[2]);
                    a_solid = tc_block_is_solid(a);
                    b_solid = tc_block_is_solid(b);

                    if (a_solid != b_solid) {
                        mask[n] = a_solid ? (int32_t)a : -(int32_t)b;
                    } else {
                        mask[n] = 0;
                    }
                }
            }

            n = 0;

            for (int j = 0; j < mask_height; j++) {
                for (int i = 0; i < mask_width;) {
                    int32_t c = mask[n];

                    if (c == 0) {
                        i++;
                        n++;
                        continue;
                    }

                    int w = 1;
                    while (i + w < mask_width && mask[n + w] == c) {
                        w++;
                    }

                    int h = 1;
                    for (; j + h < mask_height; h++) {
                        int k = 0;
                        for (; k < w; k++) {
                            if (mask[n + k + h * mask_width] != c) {
                                break;
                            }
                        }
                        if (k != w) {
                            break;
                        }
                    }

                    coordinate[u] = i;
                    coordinate[v] = j;

                    int p[3] = {
                        coordinate[0],
                        coordinate[1],
                        coordinate[2]
                    };
                    p[axis] = coordinate[axis] + 1;

                    uint16_t positions[4][3] = {
                        {(uint16_t)p[0], (uint16_t)p[1], (uint16_t)p[2]},
                        {(uint16_t)p[0], (uint16_t)p[1], (uint16_t)p[2]},
                        {(uint16_t)p[0], (uint16_t)p[1], (uint16_t)p[2]},
                        {(uint16_t)p[0], (uint16_t)p[1], (uint16_t)p[2]}
                    };
                    uint8_t uvs[4][2];
                    uint8_t shade;
                    uint8_t tile;
                    tc_vertex quad[4];

                    if (axis == 0) {
                        uvs[0][0] = 0;
                        uvs[0][1] = 0;
                        uvs[1][0] = 0;
                        uvs[1][1] = (uint8_t)w;
                        uvs[2][0] = (uint8_t)h;
                        uvs[2][1] = (uint8_t)w;
                        uvs[3][0] = (uint8_t)h;
                        uvs[3][1] = 0;
                    } else {
                        uvs[0][0] = 0;
                        uvs[0][1] = 0;
                        uvs[1][0] = (uint8_t)w;
                        uvs[1][1] = 0;
                        uvs[2][0] = (uint8_t)w;
                        uvs[2][1] = (uint8_t)h;
                        uvs[3][0] = 0;
                        uvs[3][1] = (uint8_t)h;
                    }

                    positions[1][u] = (uint16_t)(positions[1][u] + w);
                    positions[2][u] = (uint16_t)(positions[2][u] + w);
                    positions[2][v] = (uint16_t)(positions[2][v] + h);
                    positions[3][v] = (uint16_t)(positions[3][v] + h);

                    if (c < 0) {
                        uint16_t temp_position[3];
                        uint8_t temp_uv[2];

                        memcpy(temp_position, positions[1],
                               sizeof(temp_position));
                        memcpy(positions[1], positions[3],
                               sizeof(temp_position));
                        memcpy(positions[3], temp_position,
                               sizeof(temp_position));

                        memcpy(temp_uv, uvs[1], sizeof(temp_uv));
                        memcpy(uvs[1], uvs[3], sizeof(temp_uv));
                        memcpy(uvs[3], temp_uv, sizeof(temp_uv));
                    }

                    block_face_style((uint8_t)(c > 0 ? c : -c), axis,
                                     c > 0, &shade, &tile);

                    for (int vertex = 0; vertex < 4; vertex++) {
                        memcpy(quad[vertex].position, positions[vertex],
                               sizeof(quad[vertex].position));
                        memcpy(quad[vertex].uv, uvs[vertex],
                               sizeof(quad[vertex].uv));
                        quad[vertex].shade = shade;
                        quad[vertex].tile = tile;
                    }

                    if (!tc_mesh_add_quad(mesh, quad)) {
                        return 0;
                    }

                    for (int row = 0; row < h; row++) {
                        memset(&mask[n + row * mask_width], 0,
                               (size_t)w * sizeof(int32_t));
                    }

                    i += w;
                    n += w;
                }
            }

        }
    }

    stats.build_ms = (tc_time_now() - build_start) * 1000.0;
    stats.quads = mesh->quad_count;
    stats.vertices = mesh->vertex_count;
    stats.indices = mesh->index_count;

    if (stats_out != NULL) {
        *stats_out = stats;
    }

    return 1;
}

int tc_chunk_build_mesh(const tc_chunk *chunk, tc_mesh *mesh,
                        tc_mesh_stats *stats) {
    return tc_chunk_build_mesh_sampled(chunk, NULL, NULL, mesh, stats);
}
