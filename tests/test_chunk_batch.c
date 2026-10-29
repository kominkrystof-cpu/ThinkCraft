#include "thinkcraft/chunk_batch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_chunk_batch: %s\n", message);
    }
    return condition;
}

static int test_compact_preserves_slots(void) {
    enum { SLOT_COUNT = 4, USED_SLOTS = 3, COUNT = 100 };
    tc_chunk_batch batch;
    int ok = 1;
    size_t required_vertices = 0;
    size_t required_indices = 0;

    memset(&batch, 0, sizeof(batch));
    batch.slot_count = SLOT_COUNT;
    batch.vertex_capacity = 600;
    batch.index_capacity = 700;
    batch.vertex_used = USED_SLOTS * COUNT;
    batch.index_used = USED_SLOTS * COUNT;
    batch.waste_vertices = 50;
    batch.waste_indices = 50;
    batch.shadow_vertices = calloc(batch.vertex_capacity,
                                   sizeof(*batch.shadow_vertices));
    batch.shadow_indices = calloc(batch.index_capacity,
                                  sizeof(*batch.shadow_indices));
    if (!check(batch.shadow_vertices != NULL && batch.shadow_indices != NULL,
               "could not allocate shadow buffers")) {
        free(batch.shadow_vertices);
        free(batch.shadow_indices);
        return 0;
    }

    for (int slot = 0; slot < USED_SLOTS; slot++) {
        tc_chunk_batch_slot *entry = &batch.slots[slot];

        entry->used = 1;
        entry->vertex_offset = (size_t)slot * COUNT;
        entry->index_offset = (size_t)slot * COUNT;
        entry->vertex_capacity = COUNT;
        entry->index_capacity = COUNT;
        entry->vertex_count = COUNT;
        entry->index_count = COUNT;

        for (int i = 0; i < COUNT; i++) {
            tc_vertex *vertex =
                &batch.shadow_vertices[entry->vertex_offset + i];

            vertex->position[0] = (uint16_t)(slot * 1000 + i);
            vertex->position[1] = (uint16_t)slot;
            vertex->position[2] = (uint16_t)i;
            vertex->uv[0] = (uint8_t)slot;
            vertex->uv[1] = (uint8_t)i;
            vertex->shade = (uint8_t)(slot + 1);
            vertex->tile = (uint8_t)slot;
            batch.shadow_indices[entry->index_offset + i] =
                (uint32_t)(entry->vertex_offset + i);
        }
    }

    batch.slots[3].used = 0;
    batch.slots[3].vertex_count = 7;
    batch.slots[3].index_count = 7;
    batch.slots[3].vertex_offset = 500;
    batch.slots[3].index_offset = 600;

    tc_chunk_batch_compact_required(&batch, &required_vertices,
                                    &required_indices);
    ok = ok && check(required_vertices == 567,
                     "required vertex capacity is wrong")
       && check(required_indices == 663,
                "required index capacity is wrong")
       && check(tc_chunk_batch_compact_cpu(&batch),
                "CPU compaction failed");

    ok = ok && check(batch.vertex_used == required_vertices,
                     "compacted vertex count is wrong")
       && check(batch.index_used == required_indices,
                "compacted index count is wrong")
       && check(batch.waste_vertices == 0 && batch.waste_indices == 0,
                "compaction did not clear waste")
       && check(batch.slots[3].used == 0
                && batch.slots[3].vertex_count == 0
                && batch.slots[3].index_count == 0,
                "unused slot was not cleared");

    for (int slot = 0; slot < USED_SLOTS; slot++) {
        const tc_chunk_batch_slot *entry = &batch.slots[slot];
        size_t expected_vertex_offset = (size_t)slot * 189;
        size_t expected_index_offset = (size_t)slot * 221;

        ok = ok && check(entry->vertex_offset == expected_vertex_offset,
                         "slot vertex offset is wrong")
           && check(entry->index_offset == expected_index_offset,
                    "slot index offset is wrong")
           && check(entry->vertex_capacity == 189,
                    "slot vertex capacity is wrong")
           && check(entry->index_capacity == 221,
                    "slot index capacity is wrong");

        for (int i = 0; i < COUNT; i++) {
            const tc_vertex *vertex =
                &batch.shadow_vertices[entry->vertex_offset + i];

            ok = ok && check(vertex->position[0]
                             == (uint16_t)(slot * 1000 + i),
                             "compaction corrupted slot vertex data")
               && check(vertex->position[1] == (uint16_t)slot,
                        "compaction mixed vertex slots")
               && check(vertex->uv[0] == (uint8_t)slot,
                        "compaction corrupted vertex UVs")
               && check(vertex->tile == (uint8_t)slot,
                        "compaction corrupted vertex tiles")
               && check(batch.shadow_indices[entry->index_offset + i]
                        == (uint32_t)(entry->vertex_offset + i),
                        "compaction corrupted slot indices");
        }
    }

    free(batch.shadow_vertices);
    free(batch.shadow_indices);
    return ok;
}

static int test_compact_rejects_small_buffers(void) {
    tc_chunk_batch batch;
    int ok = 1;

    memset(&batch, 0, sizeof(batch));
    batch.slot_count = 1;
    batch.vertex_capacity = 10;
    batch.index_capacity = 10;
    batch.vertex_used = 10;
    batch.index_used = 10;
    batch.shadow_vertices = calloc(10, sizeof(*batch.shadow_vertices));
    batch.shadow_indices = calloc(10, sizeof(*batch.shadow_indices));
    if (!check(batch.shadow_vertices != NULL && batch.shadow_indices != NULL,
               "could not allocate small shadow buffers")) {
        free(batch.shadow_vertices);
        free(batch.shadow_indices);
        return 0;
    }

    batch.slots[0].used = 1;
    batch.slots[0].vertex_count = 10;
    batch.slots[0].index_count = 10;

    ok = ok && check(!tc_chunk_batch_compact_cpu(&batch),
                     "compaction accepted undersized buffers")
       && check(batch.vertex_used == 10 && batch.index_used == 10,
                "failed compaction changed used counts");

    free(batch.shadow_vertices);
    free(batch.shadow_indices);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_compact_preserves_slots();
    ok = ok && test_compact_rejects_small_buffers();

    if (!ok) {
        return 1;
    }

    printf("test_chunk_batch: all checks passed\n");
    return 0;
}
