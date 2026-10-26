#ifndef THINKCRAFT_WORLD_H
#define THINKCRAFT_WORLD_H

#include "thinkcraft/arena.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/chunk_mesh.h"

#define TC_WORLD_DEFAULT_RADIUS 4
#define TC_WORLD_DEFAULT_SIZE (TC_WORLD_DEFAULT_RADIUS * 2 + 1)

typedef struct {
    tc_chunk chunk;
    int chunk_x;
    int chunk_z;
    int loaded;
    int dirty;
} tc_world_slot;

typedef struct {
    int radius;
    int size;
    int center_chunk_x;
    int center_chunk_z;
    int generate_demo;
    tc_world_slot *slots;
} tc_world;

int tc_world_init(tc_world *world, tc_arena *arena, int radius);
void tc_world_shutdown(tc_world *world);
int tc_world_update(tc_world *world, int center_chunk_x, int center_chunk_z);
void tc_world_generate_demo(tc_world *world);
tc_chunk *tc_world_chunk_at(const tc_world *world, int chunk_x, int chunk_z);
uint8_t tc_world_get_block(const tc_world *world, int x, int y, int z);
int tc_world_set_block(tc_world *world, int x, int y, int z, uint8_t block);
int tc_world_build_chunk_mesh(const tc_world *world, int chunk_x, int chunk_z,
                              tc_mesh *mesh, tc_mesh_stats *stats);

#endif
