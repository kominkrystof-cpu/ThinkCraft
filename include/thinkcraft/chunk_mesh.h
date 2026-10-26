#ifndef THINKCRAFT_CHUNK_MESH_H
#define THINKCRAFT_CHUNK_MESH_H

#include <stddef.h>

#include "thinkcraft/chunk.h"
#include "thinkcraft/mesh.h"

typedef struct {
    size_t solid_blocks;
    size_t visible_faces;
    size_t quads;
    size_t vertices;
    size_t indices;
    double scan_ms;
    double build_ms;
} tc_mesh_stats;

int tc_chunk_build_mesh(const tc_chunk *chunk, tc_mesh *mesh,
                        tc_mesh_stats *stats);
int tc_chunk_build_mesh_sampled(const tc_chunk *chunk,
                                tc_block_sampler sampler, const void *context,
                                tc_mesh *mesh, tc_mesh_stats *stats);

#endif
