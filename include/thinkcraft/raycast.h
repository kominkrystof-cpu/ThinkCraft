#ifndef THINKCRAFT_RAYCAST_H
#define THINKCRAFT_RAYCAST_H

#include "thinkcraft/world.h"

typedef struct {
    int x;
    int y;
    int z;
    int previous_x;
    int previous_y;
    int previous_z;
    uint8_t block;
} tc_raycast_hit;

int tc_world_raycast(const tc_world *world, const float origin[3],
                     const float direction[3], float max_distance,
                     tc_raycast_hit *hit);

#endif
