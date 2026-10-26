#ifndef THINKCRAFT_WORLD_H
#define THINKCRAFT_WORLD_H

#include "thinkcraft/arena.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/chunk_mesh.h"

#define TC_WORLD_DEFAULT_CHUNKS_X 5
#define TC_WORLD_DEFAULT_CHUNKS_Z 5

typedef struct {
    int chunks_x;
    int chunks_z;
    tc_chunk *chunks;
} tc_world;

int tc_world_init(tc_world *world, tc_arena *arena, int chunks_x, int chunks_z);
void tc_world_shutdown(tc_world *world);
void tc_world_generate_demo(tc_world *world);
tc_chunk *tc_world_chunk_at(const tc_world *world, int chunk_x, int chunk_z);
uint8_t tc_world_get_block(const tc_world *world, int x, int y, int z);
int tc_world_set_block(tc_world *world, int x, int y, int z, uint8_t block);
int tc_world_build_chunk_mesh(const tc_world *world, int chunk_x, int chunk_z,
                              tc_mesh *mesh, tc_mesh_stats *stats);

#endif
