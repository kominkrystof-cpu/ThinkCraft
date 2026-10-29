#include "thinkcraft/visibility.h"

#include "thinkcraft/block.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/raycast.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TC_VISIBILITY_SAMPLE_COUNT 9
#define TC_VISIBILITY_EPSILON 0.001f
#define TC_VISIBILITY_NEAR_SKIP 0.5f
#define TC_VISIBILITY_MIN_DISTANCE 24.0f

static int ray_aabb_entry(const float origin[3], const float direction[3],
                          const float min[3], const float max[3],
                          float *entry) {
    float t_min = 0.0f;
    float t_max = 1.0f;

    for (int axis = 0; axis < 3; axis++) {
        float o = origin[axis];
        float d = direction[axis];
        float lo = min[axis];
        float hi = max[axis];

        if (fabsf(d) < 1.0e-9f) {
            if (o < lo || o > hi) {
                return 0;
            }
            continue;
        }

        {
            float t1 = (lo - o) / d;
            float t2 = (hi - o) / d;

            if (t1 > t2) {
                float swap = t1;
                t1 = t2;
                t2 = swap;
            }
            if (t1 > t_min) {
                t_min = t1;
            }
            if (t2 < t_max) {
                t_max = t2;
            }
            if (t_min > t_max) {
                return 0;
            }
        }
    }

    *entry = t_min;
    return 1;
}

static void chunk_bounds(const tc_world_slot *slot, float min[3],
                         float max[3]) {
    min[0] = (float)slot->chunk.origin_x;
    min[1] = 0.0f;
    min[2] = (float)slot->chunk.origin_z;
    max[0] = min[0] + (float)TC_CHUNK_SIZE_X;
    max[1] = (float)(slot->chunk.max_y + 1);
    max[2] = min[2] + (float)TC_CHUNK_SIZE_Z;
}

static void collect_samples(const float min[3], const float max[3],
                            float samples[TC_VISIBILITY_SAMPLE_COUNT][3]) {
    int sample = 0;

    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            for (int z = 0; z < 2; z++) {
                samples[sample][0] = x ? max[0] : min[0];
                samples[sample][1] = y ? max[1] : min[1];
                samples[sample][2] = z ? max[2] : min[2];
                sample++;
            }
        }
    }

    samples[sample][0] = (min[0] + max[0]) * 0.5f;
    samples[sample][1] = (min[1] + max[1]) * 0.5f;
    samples[sample][2] = (min[2] + max[2]) * 0.5f;
}

static int sample_occluded(const tc_world *world, const tc_world_slot *slot,
                           const float eye[3], const float min[3],
                           const float max[3], const float sample[3]) {
    float direction[3];
    float length_sq;
    float length;
    float entry;
    float max_distance;
    tc_raycast_hit hit;

    direction[0] = sample[0] - eye[0];
    direction[1] = sample[1] - eye[1];
    direction[2] = sample[2] - eye[2];
    length_sq = direction[0] * direction[0]
              + direction[1] * direction[1]
              + direction[2] * direction[2];
    if (length_sq < 1.0e-8f) {
        return 0;
    }
    length = sqrtf(length_sq);

    if (!ray_aabb_entry(eye, direction, min, max, &entry)
        || entry <= TC_VISIBILITY_EPSILON) {
        return 0;
    }

    max_distance = entry * length - TC_VISIBILITY_EPSILON;
    if (max_distance <= TC_VISIBILITY_NEAR_SKIP) {
        return 0;
    }

    if (!tc_world_raycast(world, eye, direction, max_distance, &hit)) {
        return 0;
    }

    if (hit.x >= slot->chunk.origin_x
        && hit.x < slot->chunk.origin_x + TC_CHUNK_SIZE_X
        && hit.z >= slot->chunk.origin_z
        && hit.z < slot->chunk.origin_z + TC_CHUNK_SIZE_Z
        && hit.y >= 0 && hit.y < TC_CHUNK_SIZE_Y) {
        return 0;
    }

    return 1;
}

int tc_visibility_chunk_occluded(const tc_world *world, size_t index,
                                 const float eye[3]) {
    const tc_world_slot *slot;
    float min[3];
    float max[3];
    float samples[TC_VISIBILITY_SAMPLE_COUNT][3];
    int eye_x;
    int eye_y;
    int eye_z;

    if (world == NULL || eye == NULL
        || index >= (size_t)world->size * (size_t)world->size) {
        return 0;
    }

    slot = &world->slots[index];
    if (!slot->loaded || slot->chunk.max_y < 0) {
        return 0;
    }

    chunk_bounds(slot, min, max);
    if (eye[0] >= min[0] && eye[0] <= max[0]
        && eye[1] >= min[1] && eye[1] <= max[1]
        && eye[2] >= min[2] && eye[2] <= max[2]) {
        return 0;
    }

    {
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;

        if (eye[0] < min[0]) {
            dx = min[0] - eye[0];
        } else if (eye[0] > max[0]) {
            dx = eye[0] - max[0];
        }
        if (eye[1] < min[1]) {
            dy = min[1] - eye[1];
        } else if (eye[1] > max[1]) {
            dy = eye[1] - max[1];
        }
        if (eye[2] < min[2]) {
            dz = min[2] - eye[2];
        } else if (eye[2] > max[2]) {
            dz = eye[2] - max[2];
        }
        if (dx * dx + dy * dy + dz * dz
            < TC_VISIBILITY_MIN_DISTANCE * TC_VISIBILITY_MIN_DISTANCE) {
            return 0;
        }
    }

    eye_x = (int)floorf(eye[0]);
    eye_y = (int)floorf(eye[1]);
    eye_z = (int)floorf(eye[2]);
    if (tc_block_is_solid(tc_world_get_block(world, eye_x, eye_y, eye_z))) {
        return 0;
    }

    /* Cave visibility is too complex for sparse sample rays: a small opening
     * between samples can reveal a chunk that the test would cull. Disable
     * occlusion culling whenever there is terrain directly overhead. */
    for (int y = eye_y + 1; y < TC_CHUNK_SIZE_Y && y <= eye_y + 32; y++) {
        if (tc_block_is_solid(tc_world_get_block(world, eye_x, y, eye_z))) {
            return 0;
        }
    }

    collect_samples(min, max, samples);
    for (int sample = 0; sample < TC_VISIBILITY_SAMPLE_COUNT; sample++) {
        if (!sample_occluded(world, slot, eye, min, max, samples[sample])) {
            return 0;
        }
    }

    return 1;
}

static int camera_jumped(const tc_visibility_scheduler *scheduler,
                         const float eye[3], const float forward[3]) {
    float dx = eye[0] - scheduler->eye[0];
    float dy = eye[1] - scheduler->eye[1];
    float dz = eye[2] - scheduler->eye[2];
    float dot;

    if (dx * dx + dy * dy + dz * dz > 144.0f) {
        return 1;
    }

    dot = forward[0] * scheduler->forward[0]
        + forward[1] * scheduler->forward[1]
        + forward[2] * scheduler->forward[2];
    return dot < 0.98f;
}

void tc_visibility_scheduler_init(tc_visibility_scheduler *scheduler,
                                  int budget) {
    if (scheduler == NULL) {
        return;
    }

    memset(scheduler, 0, sizeof(*scheduler));
    if (budget < 1) {
        budget = 1;
    } else if (budget > TC_VISIBILITY_MAX_CHUNKS) {
        budget = TC_VISIBILITY_MAX_CHUNKS;
    }
    scheduler->budget = budget;
}

void tc_visibility_scheduler_begin_frame(tc_visibility_scheduler *scheduler,
                                         const float eye[3],
                                         const float forward[3],
                                         int slot_count, int center_chunk_x,
                                         int center_chunk_z) {
    if (scheduler == NULL || eye == NULL || forward == NULL) {
        return;
    }

    if (slot_count <= 0 || slot_count > TC_VISIBILITY_MAX_CHUNKS) {
        scheduler->slot_count = 0;
        return;
    }

    if (scheduler->slot_count != slot_count
        || scheduler->center_chunk_x != center_chunk_x
        || scheduler->center_chunk_z != center_chunk_z) {
        memset(scheduler->occluded, 0, sizeof(scheduler->occluded));
        scheduler->cursor = 0;
        scheduler->slot_count = slot_count;
        scheduler->center_chunk_x = center_chunk_x;
        scheduler->center_chunk_z = center_chunk_z;
    } else if (camera_jumped(scheduler, eye, forward)) {
        memset(scheduler->occluded, 0, sizeof(scheduler->occluded));
        scheduler->cursor = 0;
    }

    memcpy(scheduler->eye, eye, sizeof(scheduler->eye));
    memcpy(scheduler->forward, forward, sizeof(scheduler->forward));
    scheduler->window_start = scheduler->cursor;
}

int tc_visibility_scheduler_visible(tc_visibility_scheduler *scheduler,
                                    const tc_world *world, size_t index,
                                    const float eye[3]) {
    int offset;
    int occluded;

    if (scheduler == NULL || world == NULL || eye == NULL
        || scheduler->slot_count <= 0
        || index >= (size_t)scheduler->slot_count) {
        return 1;
    }

    offset = (int)((index + (size_t)scheduler->slot_count
                    - (size_t)scheduler->window_start)
                   % (size_t)scheduler->slot_count);
    if (offset >= scheduler->budget) {
        return scheduler->occluded[index] ? 0 : 1;
    }

    occluded = tc_visibility_chunk_occluded(world, index, eye);
    scheduler->occluded[index] = (unsigned char)occluded;
    return !occluded;
}

void tc_visibility_scheduler_end_frame(tc_visibility_scheduler *scheduler) {
    if (scheduler == NULL || scheduler->slot_count <= 0) {
        return;
    }

    scheduler->cursor = (scheduler->cursor + scheduler->budget)
                      % scheduler->slot_count;
}
