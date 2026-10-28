#include "thinkcraft/chunk_batch.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/log.h"

#include <glad/gl.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TC_BATCH_INITIAL_VERTICES (1u << 18)
#define TC_BATCH_INITIAL_INDICES (1u << 19)
#define TC_BATCH_MIN_GROWTH 1024

static void configure_batch_mesh(tc_chunk_batch *batch) {
    glBindVertexArray(batch->vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glVertexAttribPointer(0, 3, GL_UNSIGNED_SHORT, GL_FALSE,
                          sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, shade));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, tile));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    glBindVertexArray(0);
}

int tc_chunk_batch_init(tc_chunk_batch *batch, int slot_count) {
    if (batch == NULL || slot_count <= 0
        || slot_count > TC_CHUNK_BATCH_MAX_SLOTS) {
        return 0;
    }

    memset(batch, 0, sizeof(*batch));
    batch->slot_count = slot_count;
    batch->bias_x = TC_CHUNK_BATCH_BIAS;
    batch->bias_z = TC_CHUNK_BATCH_BIAS;
    batch->vertex_capacity = TC_BATCH_INITIAL_VERTICES;
    batch->index_capacity = TC_BATCH_INITIAL_INDICES;

    batch->shadow_vertices = calloc(batch->vertex_capacity,
                                    sizeof(tc_vertex));
    batch->shadow_indices = calloc(batch->index_capacity,
                                   sizeof(uint32_t));
    if (batch->shadow_vertices == NULL || batch->shadow_indices == NULL) {
        free(batch->shadow_vertices);
        free(batch->shadow_indices);
        memset(batch, 0, sizeof(*batch));
        return 0;
    }

    glGenVertexArrays(1, &batch->vao);
    glGenBuffers(1, &batch->vbo);
    glGenBuffers(1, &batch->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(batch->vertex_capacity * sizeof(tc_vertex)),
                 NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(batch->index_capacity * sizeof(uint32_t)),
                 NULL, GL_DYNAMIC_DRAW);

    configure_batch_mesh(batch);
    return 1;
}

void tc_chunk_batch_shutdown(tc_chunk_batch *batch) {
    if (batch == NULL) {
        return;
    }

    if (batch->vao != 0) {
        glDeleteVertexArrays(1, &batch->vao);
    }
    if (batch->vbo != 0) {
        glDeleteBuffers(1, &batch->vbo);
    }
    if (batch->ebo != 0) {
        glDeleteBuffers(1, &batch->ebo);
    }

    free(batch->shadow_vertices);
    free(batch->shadow_indices);
    memset(batch, 0, sizeof(*batch));
}

static size_t next_capacity(size_t current, size_t needed) {
    size_t capacity = current > 0 ? current : TC_BATCH_MIN_GROWTH;

    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            return needed;
        }
        capacity *= 2;
    }

    return capacity;
}

static int ensure_capacity(tc_chunk_batch *batch, size_t vertex_capacity,
                           size_t index_capacity) {
    if (vertex_capacity <= batch->vertex_capacity
        && index_capacity <= batch->index_capacity) {
        return 1;
    }

    if (vertex_capacity > batch->vertex_capacity) {
        size_t capacity = next_capacity(batch->vertex_capacity,
                                        vertex_capacity);
        tc_vertex *vertices;

        if (capacity > SIZE_MAX / sizeof(tc_vertex)) {
            return 0;
        }
        vertices = realloc(batch->shadow_vertices,
                           capacity * sizeof(tc_vertex));
        if (vertices == NULL) {
            return 0;
        }
        batch->shadow_vertices = vertices;
        batch->vertex_capacity = capacity;
    }

    if (index_capacity > batch->index_capacity) {
        size_t capacity = next_capacity(batch->index_capacity, index_capacity);
        uint32_t *indices;

        if (capacity > SIZE_MAX / sizeof(uint32_t)) {
            return 0;
        }
        indices = realloc(batch->shadow_indices,
                          capacity * sizeof(uint32_t));
        if (indices == NULL) {
            return 0;
        }
        batch->shadow_indices = indices;
        batch->index_capacity = capacity;
    }

    glBindVertexArray(batch->vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(batch->vertex_capacity * sizeof(tc_vertex)),
                 NULL, GL_DYNAMIC_DRAW);
    if (batch->vertex_used > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(batch->vertex_used * sizeof(tc_vertex)),
                        batch->shadow_vertices);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(batch->index_capacity * sizeof(uint32_t)),
                 NULL, GL_DYNAMIC_DRAW);
    if (batch->index_used > 0) {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(batch->index_used * sizeof(uint32_t)),
                        batch->shadow_indices);
    }
    glBindVertexArray(0);
    return 1;
}

static int upload_ranges(tc_chunk_batch *batch,
                         const tc_chunk_batch_slot *slot) {
    glBindVertexArray(batch->vao);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glBufferSubData(GL_ARRAY_BUFFER,
                    (GLintptr)(slot->vertex_offset * sizeof(tc_vertex)),
                    (GLsizeiptr)(slot->vertex_count * sizeof(tc_vertex)),
                    &batch->shadow_vertices[slot->vertex_offset]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                    (GLintptr)(slot->index_offset * sizeof(uint32_t)),
                    (GLsizeiptr)(slot->index_count * sizeof(uint32_t)),
                    &batch->shadow_indices[slot->index_offset]);

    glBindVertexArray(0);
    return 1;
}

static void upload_full(tc_chunk_batch *batch) {
    glBindVertexArray(batch->vao);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    if (batch->vertex_used > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(batch->vertex_used * sizeof(tc_vertex)),
                        batch->shadow_vertices);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    if (batch->index_used > 0) {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(batch->index_used * sizeof(uint32_t)),
                        batch->shadow_indices);
    }

    glBindVertexArray(0);
}

static void sort_slots_by_offset(const tc_chunk_batch *batch, int *order) {
    int count = 0;

    for (int index = 0; index < batch->slot_count; index++) {
        if (batch->slots[index].used
            && batch->slots[index].index_count > 0) {
            order[count++] = index;
        }
    }

    for (int index = 1; index < count; index++) {
        int item = order[index];
        size_t offset = batch->slots[item].vertex_offset;
        int position = index;

        while (position > 0
               && offset
                  < batch->slots[order[position - 1]].vertex_offset) {
            order[position] = order[position - 1];
            position--;
        }
        order[position] = item;
    }

    order[count] = -1;
}

static int compact(tc_chunk_batch *batch) {
    size_t vertex_used = 0;
    size_t index_used = 0;
    size_t new_vertex_offset[TC_CHUNK_BATCH_MAX_SLOTS];
    size_t new_index_offset[TC_CHUNK_BATCH_MAX_SLOTS];
    size_t new_vertex_capacity[TC_CHUNK_BATCH_MAX_SLOTS];
    size_t new_index_capacity[TC_CHUNK_BATCH_MAX_SLOTS];
    int order[TC_CHUNK_BATCH_MAX_SLOTS + 1];
    unsigned char keep[TC_CHUNK_BATCH_MAX_SLOTS] = {0};
    int count = 0;

    sort_slots_by_offset(batch, order);

    for (int position = 0; order[position] >= 0; position++) {
        int index = order[position];
        const tc_chunk_batch_slot *slot = &batch->slots[index];
        size_t vertex_capacity = (size_t)slot->vertex_count
                               + ((size_t)slot->vertex_count >> 2) + 64;
        size_t index_capacity = (size_t)slot->index_count
                              + ((size_t)slot->index_count >> 2) + 96;

        new_vertex_offset[index] = vertex_used;
        new_index_offset[index] = index_used;
        new_vertex_capacity[index] = vertex_capacity;
        new_index_capacity[index] = index_capacity;
        vertex_used += vertex_capacity;
        index_used += index_capacity;
        keep[index] = 1;
        count++;
    }

    if (!ensure_capacity(batch, vertex_used, index_used)) {
        return 0;
    }

    for (int position = 0; position < count; position++) {
        int index = order[position];
        tc_chunk_batch_slot *slot = &batch->slots[index];
        size_t old_vertex_offset = slot->vertex_offset;
        size_t old_index_offset = slot->index_offset;
        size_t vertex_offset = new_vertex_offset[index];
        size_t index_offset = new_index_offset[index];
        uint32_t delta = (uint32_t)vertex_offset
                       - (uint32_t)old_vertex_offset;

        if (vertex_offset != old_vertex_offset && slot->vertex_count > 0) {
            memmove(&batch->shadow_vertices[vertex_offset],
                    &batch->shadow_vertices[old_vertex_offset],
                    (size_t)slot->vertex_count * sizeof(tc_vertex));
        }

        for (unsigned int i = 0; i < slot->index_count; i++) {
            batch->shadow_indices[index_offset + i] =
                batch->shadow_indices[old_index_offset + i] + delta;
        }

        slot->vertex_offset = vertex_offset;
        slot->index_offset = index_offset;
        slot->vertex_capacity = new_vertex_capacity[index];
        slot->index_capacity = new_index_capacity[index];
        slot->used = 1;
    }

    for (int index = 0; index < batch->slot_count; index++) {
        if (!keep[index]) {
            batch->slots[index].used = 0;
            batch->slots[index].vertex_count = 0;
            batch->slots[index].index_count = 0;
        }
    }

    batch->vertex_used = vertex_used;
    batch->index_used = index_used;
    batch->waste_vertices = 0;
    batch->waste_indices = 0;
    upload_full(batch);
    return 1;
}

int tc_chunk_batch_upload(tc_chunk_batch *batch, int slot,
                          const tc_mesh *mesh) {
    tc_chunk_batch_slot *entry;
    int64_t biased_x;
    int64_t biased_z;
    uint32_t bias_x;
    uint32_t bias_z;
    int reuse;
    unsigned int vertex_count;
    unsigned int index_count;

    if (batch == NULL || slot < 0 || slot >= batch->slot_count) {
        return 0;
    }

    entry = &batch->slots[slot];
    if (mesh == NULL || mesh->vertex_count == 0 || mesh->index_count == 0) {
        tc_chunk_batch_clear(batch, slot);
        return 1;
    }

    if (mesh->vertex_count > UINT32_MAX || mesh->index_count > UINT32_MAX) {
        return 0;
    }

    biased_x = (int64_t)(int32_t)mesh->origin[0] + batch->bias_x;
    biased_z = (int64_t)(int32_t)mesh->origin[2] + batch->bias_z;
    if (biased_x < 0 || biased_x + TC_CHUNK_SIZE_X > 0xffff
        || biased_z < 0 || biased_z + TC_CHUNK_SIZE_Z > 0xffff) {
        return 0;
    }
    bias_x = (uint32_t)biased_x;
    bias_z = (uint32_t)biased_z;

    vertex_count = (unsigned int)mesh->vertex_count;
    index_count = (unsigned int)mesh->index_count;
    reuse = entry->used && vertex_count <= entry->vertex_capacity
         && index_count <= entry->index_capacity;

    if (!reuse) {
        size_t vertex_capacity = (size_t)vertex_count
                               + ((size_t)vertex_count >> 2) + 64;
        size_t index_capacity = (size_t)index_count
                              + ((size_t)index_count >> 2) + 96;
        size_t vertex_offset = batch->vertex_used;
        size_t index_offset = batch->index_used;

        if (!ensure_capacity(batch, vertex_offset + vertex_capacity,
                             index_offset + index_capacity)) {
            return 0;
        }

        if (entry->used) {
            batch->waste_vertices += entry->vertex_capacity;
            batch->waste_indices += entry->index_capacity;
        }

        entry->vertex_offset = vertex_offset;
        entry->vertex_capacity = vertex_capacity;
        entry->index_offset = index_offset;
        entry->index_capacity = index_capacity;
        entry->used = 1;
        batch->vertex_used += vertex_capacity;
        batch->index_used += index_capacity;
    }

    tc_batch_bake_vertices(&batch->shadow_vertices[entry->vertex_offset],
                           mesh->vertices, vertex_count, bias_x, bias_z);

    {
        uint32_t base = (uint32_t)entry->vertex_offset;
        uint32_t *destination = &batch->shadow_indices[entry->index_offset];

        for (unsigned int index = 0; index < index_count; index++) {
            destination[index] = mesh->indices[index] + base;
        }
    }

    entry->vertex_count = vertex_count;
    entry->index_count = index_count;
    upload_ranges(batch, entry);

    if (batch->waste_vertices * 2 > batch->vertex_used
        || batch->waste_indices * 2 > batch->index_used) {
        if (!compact(batch)) {
            tc_log_error("chunk batch compaction failed");
            return 0;
        }
    }

    return 1;
}

void tc_chunk_batch_clear(tc_chunk_batch *batch, int slot) {
    if (batch == NULL || slot < 0 || slot >= batch->slot_count) {
        return;
    }

    batch->slots[slot].vertex_count = 0;
    batch->slots[slot].index_count = 0;
}

unsigned int tc_chunk_batch_index_count(const tc_chunk_batch *batch,
                                        int slot) {
    if (batch == NULL || slot < 0 || slot >= batch->slot_count) {
        return 0;
    }

    return batch->slots[slot].index_count;
}

void tc_chunk_batch_origin(const tc_chunk_batch *batch, float origin[3]) {
    if (batch == NULL || origin == NULL) {
        return;
    }

    origin[0] = (float)-batch->bias_x;
    origin[1] = 0.0f;
    origin[2] = (float)-batch->bias_z;
}

int tc_chunk_batch_should_rebase(const tc_chunk_batch *batch, int center_x,
                                 int center_z) {
    int64_t biased_x;
    int64_t biased_z;

    if (batch == NULL) {
        return 0;
    }

    biased_x = (int64_t)center_x + batch->bias_x;
    biased_z = (int64_t)center_z + batch->bias_z;

    return biased_x < 8192 || biased_x > 57344
        || biased_z < 8192 || biased_z > 57344;
}

int tc_chunk_batch_rebase(tc_chunk_batch *batch, int center_x, int center_z) {
    int32_t new_bias_x;
    int32_t new_bias_z;
    int32_t delta_x;
    int32_t delta_z;

    if (batch == NULL) {
        return 0;
    }

    new_bias_x = TC_CHUNK_BATCH_BIAS - center_x;
    new_bias_z = TC_CHUNK_BATCH_BIAS - center_z;
    delta_x = new_bias_x - batch->bias_x;
    delta_z = new_bias_z - batch->bias_z;

    if (delta_x == 0 && delta_z == 0) {
        return 1;
    }

    for (int slot = 0; slot < batch->slot_count; slot++) {
        const tc_chunk_batch_slot *entry = &batch->slots[slot];

        if (!entry->used) {
            continue;
        }

        for (unsigned int index = 0; index < entry->vertex_count; index++) {
            tc_vertex *vertex =
                &batch->shadow_vertices[entry->vertex_offset + index];

            vertex->position[0] =
                (uint16_t)(vertex->position[0] + delta_x);
            vertex->position[2] =
                (uint16_t)(vertex->position[2] + delta_z);
        }
    }

    batch->bias_x = new_bias_x;
    batch->bias_z = new_bias_z;
    upload_full(batch);
    return 1;
}

void tc_chunk_batch_draw(const tc_chunk_batch *batch, const int *slots,
                         size_t count) {
    GLsizei counts[TC_CHUNK_BATCH_MAX_SLOTS];
    const void *offsets[TC_CHUNK_BATCH_MAX_SLOTS];
    GLsizei draw_count = 0;

    if (batch == NULL || slots == NULL || count == 0) {
        return;
    }

    for (size_t index = 0; index < count; index++) {
        int slot = slots[index];
        const tc_chunk_batch_slot *entry;

        if (slot < 0 || slot >= batch->slot_count) {
            continue;
        }

        entry = &batch->slots[slot];
        if (entry->index_count == 0) {
            continue;
        }

        counts[draw_count] = (GLsizei)entry->index_count;
        offsets[draw_count] = (const void *)(uintptr_t)
            (entry->index_offset * sizeof(uint32_t));
        draw_count++;
    }

    if (draw_count == 0) {
        return;
    }

    glBindVertexArray(batch->vao);
    glMultiDrawElements(GL_TRIANGLES, counts, GL_UNSIGNED_INT, offsets,
                        draw_count);
}
