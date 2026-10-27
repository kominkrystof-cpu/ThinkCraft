#include "thinkcraft/raycast.h"

#include <math.h>

static double axis_t_max(double origin, double direction, int step) {
    if (step == 0) {
        return INFINITY;
    }
    if (step > 0) {
        return (floor(origin) + 1.0 - origin) / direction;
    }
    return (floor(origin) - origin) / direction;
}

int tc_world_raycast(const tc_world *world, const float origin[3],
                     const float direction[3], float max_distance,
                     tc_raycast_hit *hit) {
    double dx;
    double dy;
    double dz;
    double length;
    double t_max_x;
    double t_max_y;
    double t_max_z;
    double t_delta_x;
    double t_delta_y;
    double t_delta_z;
    int step_x;
    int step_y;
    int step_z;
    int x;
    int y;
    int z;

    if (world == NULL || origin == NULL || direction == NULL || hit == NULL
        || max_distance <= 0.0f) {
        return 0;
    }

    dx = (double)direction[0];
    dy = (double)direction[1];
    dz = (double)direction[2];
    length = sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 1e-9) {
        return 0;
    }

    dx /= length;
    dy /= length;
    dz /= length;

    x = (int)floor((double)origin[0]);
    y = (int)floor((double)origin[1]);
    z = (int)floor((double)origin[2]);

    *hit = (tc_raycast_hit){0};

    if (tc_block_is_solid(tc_world_get_block(world, x, y, z))) {
        hit->x = x;
        hit->y = y;
        hit->z = z;
        hit->previous_x = x;
        hit->previous_y = y;
        hit->previous_z = z;
        hit->block = tc_world_get_block(world, x, y, z);
        hit->distance = 0.0f;
        return 1;
    }

    step_x = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
    step_y = dy > 0.0 ? 1 : (dy < 0.0 ? -1 : 0);
    step_z = dz > 0.0 ? 1 : (dz < 0.0 ? -1 : 0);

    t_delta_x = step_x != 0 ? fabs(1.0 / dx) : INFINITY;
    t_delta_y = step_y != 0 ? fabs(1.0 / dy) : INFINITY;
    t_delta_z = step_z != 0 ? fabs(1.0 / dz) : INFINITY;

    t_max_x = axis_t_max((double)origin[0], dx, step_x);
    t_max_y = axis_t_max((double)origin[1], dy, step_y);
    t_max_z = axis_t_max((double)origin[2], dz, step_z);

    for (;;) {
        double distance;
        int previous_x = x;
        int previous_y = y;
        int previous_z = z;

        if (t_max_x <= t_max_y && t_max_x <= t_max_z) {
            distance = t_max_x;
            if (distance > (double)max_distance) {
                break;
            }
            x += step_x;
            t_max_x += t_delta_x;
        } else if (t_max_y <= t_max_z) {
            distance = t_max_y;
            if (distance > (double)max_distance) {
                break;
            }
            y += step_y;
            t_max_y += t_delta_y;
        } else {
            distance = t_max_z;
            if (distance > (double)max_distance) {
                break;
            }
            z += step_z;
            t_max_z += t_delta_z;
        }

        {
            uint8_t block = tc_world_get_block(world, x, y, z);

            if (tc_block_is_solid(block)) {
                hit->x = x;
                hit->y = y;
                hit->z = z;
                hit->previous_x = previous_x;
                hit->previous_y = previous_y;
                hit->previous_z = previous_z;
                hit->block = block;
                hit->distance = (float)distance;
                return 1;
            }
        }
    }

    return 0;
}
