#ifndef THINKCRAFT_VISIBILITY_H
#define THINKCRAFT_VISIBILITY_H

#include "thinkcraft/world.h"

#define TC_VISIBILITY_MAX_CHUNKS 512

/* Returns 1 when every sample point of the chunk's bounding box is hidden by
 * solid blocks outside that chunk. The test is intentionally conservative:
 * any unoccluded sample keeps the chunk visible. */
int tc_visibility_chunk_occluded(const tc_world *world, size_t index,
                                 const float eye[3]);

/* Occlusion tests are spread over several frames. Until a chunk has been
 * tested it is treated as visible, and a camera jump invalidates cached
 * occluded results so stale culling can never hide geometry. */
typedef struct {
    unsigned char occluded[TC_VISIBILITY_MAX_CHUNKS];
    int cursor;
    int window_start;
    int budget;
    int slot_count;
    int center_chunk_x;
    int center_chunk_z;
    float eye[3];
    float forward[3];
} tc_visibility_scheduler;

void tc_visibility_scheduler_init(tc_visibility_scheduler *scheduler,
                                  int budget);
void tc_visibility_scheduler_begin_frame(tc_visibility_scheduler *scheduler,
                                         const float eye[3],
                                         const float forward[3],
                                         int slot_count, int center_chunk_x,
                                         int center_chunk_z);
int tc_visibility_scheduler_visible(tc_visibility_scheduler *scheduler,
                                    const tc_world *world, size_t index,
                                    const float eye[3]);
void tc_visibility_scheduler_end_frame(tc_visibility_scheduler *scheduler);

#endif
