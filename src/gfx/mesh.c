#include "thinkcraft/mesh.h"

#include <stdlib.h>
#include <string.h>

static int reserve_vertices(tc_mesh *mesh, size_t needed) {
    size_t capacity;
    tc_vertex *vertices;

    if (needed <= mesh->vertex_capacity) {
        return 1;
    }

    capacity = mesh->vertex_capacity ? mesh->vertex_capacity : 1024;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(tc_vertex)) {
        return 0;
    }

    vertices = realloc(mesh->vertices, capacity * sizeof(tc_vertex));
    if (vertices == NULL) {
        return 0;
    }

    mesh->vertices = vertices;
    mesh->vertex_capacity = capacity;
    return 1;
}

static int reserve_indices(tc_mesh *mesh, size_t needed) {
    size_t capacity;
    uint32_t *indices;

    if (needed <= mesh->index_capacity) {
        return 1;
    }

    capacity = mesh->index_capacity ? mesh->index_capacity : 1536;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(uint32_t)) {
        return 0;
    }

    indices = realloc(mesh->indices, capacity * sizeof(uint32_t));
    if (indices == NULL) {
        return 0;
    }

    mesh->indices = indices;
    mesh->index_capacity = capacity;
    return 1;
}

void tc_mesh_init(tc_mesh *mesh) {
    if (mesh != NULL) {
        memset(mesh, 0, sizeof(*mesh));
    }
}

void tc_mesh_shutdown(tc_mesh *mesh) {
    if (mesh == NULL) {
        return;
    }

    free(mesh->vertices);
    free(mesh->indices);
    memset(mesh, 0, sizeof(*mesh));
}

void tc_mesh_clear(tc_mesh *mesh) {
    if (mesh == NULL) {
        return;
    }

    mesh->vertex_count = 0;
    mesh->index_count = 0;
    mesh->quad_count = 0;
}

int tc_mesh_add_quad(tc_mesh *mesh, const tc_vertex quad[4]) {
    uint32_t base;
    uint32_t *indices;

    if (mesh == NULL || quad == NULL) {
        return 0;
    }

    if (mesh->vertex_count > (size_t)UINT32_MAX - 4
        || mesh->index_count > SIZE_MAX - 6) {
        return 0;
    }

    if (!reserve_vertices(mesh, mesh->vertex_count + 4)
        || !reserve_indices(mesh, mesh->index_count + 6)) {
        return 0;
    }

    base = (uint32_t)mesh->vertex_count;
    memcpy(&mesh->vertices[mesh->vertex_count], quad, 4 * sizeof(tc_vertex));
    mesh->vertex_count += 4;

    indices = &mesh->indices[mesh->index_count];
    indices[0] = base;
    indices[1] = base + 1;
    indices[2] = base + 2;
    indices[3] = base;
    indices[4] = base + 2;
    indices[5] = base + 3;

    mesh->index_count += 6;
    mesh->quad_count++;
    return 1;
}
