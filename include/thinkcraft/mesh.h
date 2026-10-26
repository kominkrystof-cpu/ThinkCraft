#ifndef THINKCRAFT_MESH_H
#define THINKCRAFT_MESH_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float position[3];
    float uv[2];
    uint8_t color[4];
} tc_vertex;

typedef struct {
    tc_vertex *vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    uint32_t *indices;
    size_t index_count;
    size_t index_capacity;
    size_t quad_count;
} tc_mesh;

void tc_mesh_init(tc_mesh *mesh);
void tc_mesh_shutdown(tc_mesh *mesh);
void tc_mesh_clear(tc_mesh *mesh);
int tc_mesh_add_quad(tc_mesh *mesh, const tc_vertex quad[4]);

#endif
