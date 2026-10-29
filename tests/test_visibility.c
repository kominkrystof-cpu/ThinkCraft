#include "thinkcraft/arena.h"
#include "thinkcraft/block.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/visibility.h"
#include "thinkcraft/world.h"

#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_visibility: %s\n", message);
    }
    return condition;
}

static int find_slot(const tc_world *world, int chunk_x, int chunk_z) {
    for (int index = 0; index < world->size * world->size; index++) {
        const tc_world_slot *slot = &world->slots[index];

        if (slot->loaded && slot->chunk_x == chunk_x
            && slot->chunk_z == chunk_z) {
            return index;
        }
    }

    return -1;
}

static void build_wall(tc_world *world, int max_y) {
    for (int x = -16; x < 32; x++) {
        for (int y = 0; y <= max_y; y++) {
            tc_world_set_block(world, x, y, 8, TC_BLOCK_STONE);
        }
    }
}

static int test_occluded_chunk(void) {
    tc_arena arena;
    tc_world world;
    float eye[3] = {8.0f, 70.0f, -8.0f};
    int index;
    int ok = 1;

    if (!check(tc_arena_init(&arena, 4 * 1024 * 1024),
               "arena init failed")) {
        return 0;
    }
    if (!check(tc_world_init(&world, &arena, 2), "world init failed")) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    build_wall(&world, 100);
    tc_world_set_block(&world, 8, 50, 20, TC_BLOCK_STONE);

    index = find_slot(&world, 0, 1);
    ok = ok && check(index >= 0, "candidate chunk is missing")
       && check(tc_visibility_chunk_occluded(&world, (size_t)index, eye),
                "chunk behind a wall was not culled");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_visible_chunk(void) {
    tc_arena arena;
    tc_world world;
    float eye[3] = {8.0f, 70.0f, -8.0f};
    int index;
    int ok = 1;

    if (!check(tc_arena_init(&arena, 4 * 1024 * 1024),
               "arena init failed")) {
        return 0;
    }
    if (!check(tc_world_init(&world, &arena, 2), "world init failed")) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    build_wall(&world, 40);
    tc_world_set_block(&world, 8, 50, 20, TC_BLOCK_STONE);

    index = find_slot(&world, 0, 1);
    ok = ok && check(index >= 0, "candidate chunk is missing")
       && check(!tc_visibility_chunk_occluded(&world, (size_t)index, eye),
                "chunk above the wall was incorrectly culled");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_eye_inside_chunk(void) {
    tc_arena arena;
    tc_world world;
    float eye[3] = {8.0f, 70.0f, 20.0f};
    int index;
    int ok = 1;

    if (!check(tc_arena_init(&arena, 4 * 1024 * 1024),
               "arena init failed")) {
        return 0;
    }
    if (!check(tc_world_init(&world, &arena, 2), "world init failed")) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    build_wall(&world, 100);
    tc_world_set_block(&world, 8, 50, 20, TC_BLOCK_STONE);

    index = find_slot(&world, 0, 1);
    ok = ok && check(index >= 0, "candidate chunk is missing")
       && check(!tc_visibility_chunk_occluded(&world, (size_t)index, eye),
                "chunk containing the eye was incorrectly culled");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}


static int test_scheduler_invalidates_on_jump(void) {
    tc_arena arena;
    tc_world world;
    tc_visibility_scheduler scheduler;
    float eye[3] = {8.0f, 70.0f, -8.0f};
    float moved[3] = {8.0f, 70.0f, 20.0f};
    float forward[3] = {0.0f, 0.0f, 1.0f};
    int index;
    int ok = 1;

    if (!check(tc_arena_init(&arena, 4 * 1024 * 1024),
               "arena init failed")) {
        return 0;
    }
    if (!check(tc_world_init(&world, &arena, 2), "world init failed")) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    build_wall(&world, 100);
    tc_world_set_block(&world, 8, 50, 20, TC_BLOCK_STONE);

    index = find_slot(&world, 0, 1);
    ok = ok && check(index >= 0, "candidate chunk is missing");

    tc_visibility_scheduler_init(&scheduler, TC_VISIBILITY_MAX_CHUNKS);
    tc_visibility_scheduler_begin_frame(&scheduler, eye, forward,
                                        world.size * world.size, 0, 0);
    ok = ok && check(!tc_visibility_scheduler_visible(&scheduler, &world,
                                                      (size_t)index, eye),
                     "scheduler did not cache an occluded chunk");
    tc_visibility_scheduler_end_frame(&scheduler);

    scheduler.budget = 1;
    tc_visibility_scheduler_begin_frame(&scheduler, moved, forward,
                                        world.size * world.size, 0, 0);
    ok = ok && check(tc_visibility_scheduler_visible(&scheduler, &world,
                                                     (size_t)index, moved),
                     "camera jump kept a stale occlusion result");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_occluded_chunk();
    ok = ok && test_visible_chunk();
    ok = ok && test_eye_inside_chunk();
    ok = ok && test_scheduler_invalidates_on_jump();

    if (!ok) {
        return 1;
    }

    printf("test_visibility: all checks passed\n");
    return 0;
}
