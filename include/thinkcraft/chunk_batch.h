#ifndef THINKCRAFT_CHUNK_BATCH_H
#define THINKCRAFT_CHUNK_BATCH_H

#include "thinkcraft/mesh.h"

#include <stddef.h>
#include <stdint.h>

/* Chunk vertices stay packed as chunk-local uint16 positions. The batch adds a
 * fixed bias once at upload time, so every visible chunk can be drawn from one
 * shared vertex buffer with a single glMultiDrawElements call. */
#define TC_CHUNK_BATCH_MAX_SLOTS 256
#define TC_CHUNK_BATCH_BIAS 32768

typedef struct {
    size_t vertex_offset;
    size_t vertex_capacity;
    size_t index_offset;
    size_t index_capacity;
    unsigned int vertex_count;
    unsigned int index_count;
    int used;
} tc_chunk_batch_slot;

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    tc_vertex *shadow_vertices;
    uint32_t *shadow_indices;
    size_t vertex_capacity;
    size_t vertex_used;
    size_t index_capacity;
    size_t index_used;
    size_t waste_vertices;
    size_t waste_indices;
    tc_chunk_batch_slot slots[TC_CHUNK_BATCH_MAX_SLOTS];
    int slot_count;
    int32_t bias_x;
    int32_t bias_z;
} tc_chunk_batch;

/* Copies packed vertices while adding the biased chunk origin to x and z.
 * Implemented in NASM so remeshing touches each vertex only once. */
void tc_batch_bake_vertices(tc_vertex *dst, const tc_vertex *src, size_t count,
                            uint32_t bias_x, uint32_t bias_z);

int tc_chunk_batch_init(tc_chunk_batch *batch, int slot_count);
void tc_chunk_batch_shutdown(tc_chunk_batch *batch);
int tc_chunk_batch_upload(tc_chunk_batch *batch, int slot, const tc_mesh *mesh);
void tc_chunk_batch_clear(tc_chunk_batch *batch, int slot);
unsigned int tc_chunk_batch_index_count(const tc_chunk_batch *batch,
                                        int slot);
void tc_chunk_batch_origin(const tc_chunk_batch *batch, float origin[3]);
int tc_chunk_batch_should_rebase(const tc_chunk_batch *batch, int center_x,
                                 int center_z);
int tc_chunk_batch_rebase(tc_chunk_batch *batch, int center_x, int center_z);
void tc_chunk_batch_draw(const tc_chunk_batch *batch, const int *slots,
                         size_t count);

#endif
