#include "thinkcraft/arena.h"
#include "thinkcraft/raycast.h"
#include "thinkcraft/world.h"

#include <math.h>
#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_raycast: %s\n", message);
    }
    return condition;
}

static int test_forward_hit(tc_world *world) {
    float origin[3] = {0.5f, 0.5f, 2.5f};
    float direction[3] = {0.0f, 0.0f, -1.0f};
    tc_raycast_hit hit;

    if (!check(tc_world_raycast(world, origin, direction, 6.0f, &hit),
               "forward ray missed")) {
        return 0;
    }

    return check(hit.block == TC_BLOCK_STONE, "forward hit wrong block")
        && check(fabsf(hit.distance - 1.5f) < 0.001f,
                 "forward hit wrong distance")
        && check(hit.x == 0 && hit.y == 0 && hit.z == 0,
                 "forward hit wrong block position")
        && check(hit.previous_x == 0 && hit.previous_y == 0
                 && hit.previous_z == 1,
                 "forward hit wrong placement cell");
}

static int test_negative_hit(tc_world *world) {
    float origin[3] = {-2.5f, 0.5f, 1.5f};
    float direction[3] = {0.0f, 0.0f, -1.0f};
    tc_raycast_hit hit;

    if (!check(tc_world_raycast(world, origin, direction, 8.0f, &hit),
               "negative ray missed")) {
        return 0;
    }

    return check(hit.block == TC_BLOCK_SAND, "negative hit wrong block")
        && check(hit.x == -3 && hit.y == 0 && hit.z == -2,
                 "negative hit wrong block position")
        && check(hit.previous_x == -3 && hit.previous_y == 0
                 && hit.previous_z == -1,
                 "negative hit wrong placement cell");
}

static int test_start_inside(tc_world *world) {
    float origin[3] = {3.5f, 0.5f, 3.5f};
    float direction[3] = {1.0f, 0.0f, 0.0f};
    tc_raycast_hit hit;

    if (!check(tc_world_raycast(world, origin, direction, 6.0f, &hit),
               "start-inside ray missed")) {
        return 0;
    }

    return check(hit.x == 3 && hit.y == 0 && hit.z == 3,
                 "start-inside hit wrong block")
        && check(fabsf(hit.distance) < 0.001f,
                 "start-inside hit distance should be zero")
        && check(hit.previous_x == 3 && hit.previous_y == 0
                 && hit.previous_z == 3,
                 "start-inside placement cell mismatch");
}

static int test_miss_and_range(tc_world *world) {
    float upward[3] = {0.0f, 1.0f, 0.0f};
    float downward[3] = {0.0f, 0.0f, -1.0f};
    float far_origin[3] = {5.5f, 5.5f, 9.5f};
    tc_raycast_hit hit;

    if (!check(!tc_world_raycast(world, (float[3]){0.5f, 12.5f, 0.5f},
                                 upward, 6.0f, &hit),
               "upward ray should miss")
        || !check(!tc_world_raycast(world, far_origin, downward, 4.0f, &hit),
                  "range-limited ray should miss")
        || !check(tc_world_raycast(world, far_origin, downward, 16.0f, &hit),
                  "long ray missed target")
        || !check(hit.x == 5 && hit.y == 5 && hit.z == -5,
                  "long ray hit wrong target")) {
        return 0;
    }

    return 1;
}

int main(void) {
    tc_arena arena;
    tc_world world;
    int ok = 1;

    if (!tc_arena_init(&arena, 4 * 1024 * 1024)) {
        fprintf(stderr, "test_raycast: arena init failed\n");
        return 1;
    }

    if (!tc_world_init(&world, &arena, 2)) {
        fprintf(stderr, "test_raycast: world init failed\n");
        tc_arena_shutdown(&arena);
        return 1;
    }

    ok = ok && check(tc_world_set_block(&world, 0, 0, 0, TC_BLOCK_STONE),
                     "stone placement failed");
    ok = ok && check(tc_world_set_block(&world, -3, 0, -2, TC_BLOCK_SAND),
                     "sand placement failed");
    ok = ok && check(tc_world_set_block(&world, 3, 0, 3, TC_BLOCK_DIRT),
                     "start block placement failed");
    ok = ok && check(tc_world_set_block(&world, 5, 5, -5, TC_BLOCK_STONE),
                     "far block placement failed");

    ok = ok && test_forward_hit(&world);
    ok = ok && test_negative_hit(&world);
    ok = ok && test_start_inside(&world);
    ok = ok && test_miss_and_range(&world);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);

    if (!ok) {
        return 1;
    }

    printf("test_raycast: all checks passed\n");
    return 0;
}
