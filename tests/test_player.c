#include "thinkcraft/arena.h"
#include "thinkcraft/player.h"
#include "thinkcraft/world.h"

#include <math.h>
#include <stdio.h>

#define TEST_DT (1.0f / 60.0f)

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_player: %s\n", message);
    }
    return condition;
}

static int init_flat_world(tc_arena *arena, tc_world *world) {
    if (!tc_arena_init(arena, 2 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(world, arena, 0)) {
        tc_arena_shutdown(arena);
        return 0;
    }

    for (int z = 0; z <= 4; z++) {
        for (int x = 0; x <= 4; x++) {
            if (!tc_world_set_block(world, x, 0, z, TC_BLOCK_STONE)) {
                return 0;
            }
        }
    }

    return 1;
}

static void simulate(tc_player *player, const tc_world *world,
                     tc_player_input input, int frames) {
    for (int frame = 0; frame < frames; frame++) {
        tc_player_update(player, world, &input, TEST_DT);
    }
}

static int test_gravity_and_jump(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0};
    float start_y;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    {
        float position[3] = {2.5f, 4.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    simulate(&player, &world, input, 240);
    ok = ok && check(player.on_ground, "player did not land")
      && check(fabsf(player.position[1] - 1.0f) < 0.001f,
               "player did not settle on the floor")
      && check(fabsf(player.velocity[1]) < 0.001f,
               "vertical velocity did not settle");

    start_y = player.position[1];
    input.jump = 1;
    tc_player_update(&player, &world, &input, TEST_DT);
    input.jump = 0;

    ok = ok && check(!player.on_ground, "jump did not leave the ground")
      && check(player.velocity[1] > 0.0f, "jump velocity is not positive")
      && check(player.position[1] > start_y, "jump did not move upward");

    simulate(&player, &world, input, 180);
    ok = ok && check(player.on_ground, "player did not land after jump")
      && check(fabsf(player.position[1] - 1.0f) < 0.001f,
               "player did not return to the floor");

    printf("player physics: gravity, ground contact, and jump passed\n");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_wall_collision(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {1.0f, 0.0f, 0, 0};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int z = 0; z <= 4; z++) {
        for (int y = 1; y <= 3; y++) {
            ok = ok && check(tc_world_set_block(&world, 2, y, z,
                                                TC_BLOCK_STONE),
                             "wall fill failed");
        }
    }

    {
        float position[3] = {1.2f, 1.0f, 2.5f};
        tc_player_init(&player, position, pi * 0.5f, 0.0f);
    }

    simulate(&player, &world, input, 180);

    ok = ok && check(player.position[0] > 1.4f,
                     "player did not move toward the wall")
      && check(player.position[0] < 1.71f,
               "player passed through the wall")
      && check(fabsf(player.position[1] - 1.0f) < 0.001f,
               "player climbed the wall")
      && check(player.on_ground, "player lost ground contact at the wall");

    printf("player collision: wall stopped movement at x=%.3f\n",
           player.position[0]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_block_intersection(void) {
    tc_player player;
    float position[3] = {0.0f, 0.0f, 0.0f};
    int ok = 1;

    tc_player_init(&player, position, 0.0f, 0.0f);

    ok = ok && check(tc_player_intersects_block(&player, 0, 0, 0),
                     "feet block should intersect")
      && check(tc_player_intersects_block(&player, 0, 0, -1),
               "overlapping negative block should intersect")
      && check(tc_player_intersects_block(&player, 0, 1, 0),
               "head block should intersect")
      && check(!tc_player_intersects_block(&player, 1, 0, 0),
               "adjacent x block should not intersect")
      && check(!tc_player_intersects_block(&player, 0, 0, 1),
               "adjacent z block should not intersect")
      && check(!tc_player_intersects_block(&player, 0, -1, 0),
               "block below feet should not intersect")
      && check(!tc_player_intersects_block(&player, 0, 2, 0),
               "block above head should not intersect");

    if (ok) {
        printf("player blocks: placement intersection checks passed\n");
    }

    return ok;
}

static int test_look_limits(void) {
    tc_player player;
    const float pi = 3.14159265358979323846f;
    float position[3] = {0.0f, 0.0f, 0.0f};
    int ok = 1;

    tc_player_init(&player, position, 0.0f, 0.0f);
    tc_player_look(&player, 100000.0f, 100000.0f, 0.0022f);

    ok = ok && check(player.pitch <= pi * 0.5f
                     && player.pitch >= -pi * 0.5f,
                     "pitch was not clamped")
      && check(player.yaw >= -pi && player.yaw <= pi,
               "yaw was not normalized")
      && check(player.pitch < 0.0f, "downward look did not lower pitch");

    tc_player_look(&player, 0.0f, 200000.0f, 0.0022f);
    ok = ok && check(player.pitch <= pi * 0.5f
                     && player.pitch >= -pi * 0.5f,
                     "pitch clamp failed upward");

    printf("player look: pitch clamp and yaw wrapping passed\n");
    return ok;
}

static int test_void_floor(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0};
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 0)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    {
        float position[3] = {2.5f, 8.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    simulate(&player, &world, input, 240);
    ok = ok && check(player.on_ground, "player did not reach the void floor")
      && check(fabsf(player.position[1] - TC_WORLD_FLOOR_Y) < 0.001f,
               "player fell through the void floor")
      && check(fabsf(player.velocity[1]) < 0.001f,
               "player did not stop at the void floor");

    printf("player void floor: stopped at y=%.3f\n", player.position[1]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_spawn(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int y = 1; y <= 5; y++) {
        ok = ok && check(tc_world_set_block(&world, 4, y, 4,
                                            TC_BLOCK_STONE),
                         "spawn column fill failed");
    }

    ok = ok && check(tc_player_spawn(&player, &world, 4.5f, 4.5f,
                                     0.0f, 0.0f),
                     "spawn failed")
      && check(fabsf(player.position[1] - 6.001f) < 0.001f,
               "spawn did not use the surface height");

    printf("player spawn: surface height %.3f\n", player.position[1]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_gravity_and_jump();
    ok = ok && test_wall_collision();
    ok = ok && test_block_intersection();
    ok = ok && test_look_limits();
    ok = ok && test_void_floor();
    ok = ok && test_spawn();

    if (!ok) {
        return 1;
    }

    printf("test_player: all checks passed\n");
    return 0;
}
