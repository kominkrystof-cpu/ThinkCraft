#include "thinkcraft/arena.h"
#include "thinkcraft/falling.h"
#include "thinkcraft/world.h"

#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_falling: %s\n", message);
    }
    return condition;
}

static int test_single_block(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 5, 0, 5, TC_BLOCK_STONE),
                     "could not place the support block")
      && check(tc_world_set_block(&world, 5, 5, 5, TC_BLOCK_SAND),
               "could not place suspended sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 5, 5, 5) == 1,
               "suspended sand did not start falling")
      && check(tc_falling_count(&falling) == 1,
               "wrong falling block count")
      && check(tc_world_get_block(&world, 5, 5, 5) == TC_BLOCK_AIR,
               "falling sand was not removed from the world");

    ok = ok && check(tc_falling_update(&falling, &world, 1.0f) == 1,
                     "falling sand did not land")
      && check(tc_falling_count(&falling) == 0,
               "landed block stayed in the entity list")
      && check(tc_world_get_block(&world, 5, 1, 5) == TC_BLOCK_SAND,
               "landed sand is not resting on the support")
      && check(tc_world_get_block(&world, 5, 5, 5) == TC_BLOCK_AIR,
               "old sand cell was not cleared");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_column_cascade(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 6, 0, 6, TC_BLOCK_STONE),
                     "could not place the column support");
    for (int y = 1; y <= 5; y++) {
        ok = ok && check(tc_world_set_block(&world, 6, y, 6, TC_BLOCK_SAND),
                         "could not place the sand column");
    }

    ok = ok && check(tc_world_set_block(&world, 6, 0, 6, TC_BLOCK_AIR),
                     "could not remove the column support")
      && check(tc_falling_notify_block_changed(&falling, &world, 6, 0, 6) == 5,
               "sand column did not cascade")
      && check(tc_falling_count(&falling) == 5,
               "wrong cascade entity count");

    ok = ok && check(tc_falling_update(&falling, &world, 1.0f) == 5,
                     "cascading column did not land")
      && check(tc_falling_count(&falling) == 0,
               "cascade entities were not removed");

    for (int y = 0; y < 5; y++) {
        ok = ok && check(tc_world_get_block(&world, 6, y, 6) == TC_BLOCK_SAND,
                         "cascade column did not stack on the floor");
    }

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_supported_and_settle(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 7, 0, 7, TC_BLOCK_STONE),
                     "could not place supported sand support")
      && check(tc_world_set_block(&world, 7, 1, 7, TC_BLOCK_SAND),
               "could not place supported sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 7, 1, 7) == 0,
               "supported sand started falling")
      && check(tc_falling_count(&falling) == 0,
               "supported sand created an entity");

    ok = ok && check(tc_world_set_block(&world, 8, 0, 8, TC_BLOCK_STONE),
                     "could not place settle support")
      && check(tc_world_set_block(&world, 8, 6, 8, TC_BLOCK_SAND),
               "could not place high sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 8, 6, 8) == 1,
               "high sand did not fall");

    tc_falling_settle_all(&falling, &world);
    ok = ok && check(tc_falling_count(&falling) == 0,
                     "settle did not clear the entity list")
      && check(tc_world_get_block(&world, 8, 1, 8) == TC_BLOCK_SAND,
               "settled sand is not resting on the support");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_single_block();
    ok = ok && test_column_cascade();
    ok = ok && test_supported_and_settle();

    if (!ok) {
        return 1;
    }

    printf("test_falling: all checks passed\n");
    return 0;
}
