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

static void simulate_mode(tc_player *player, const tc_world *world,
                          tc_player_input input, int frames,
                          tc_game_mode mode) {
    for (int frame = 0; frame < frames; frame++) {
        tc_player_update_mode(player, world, &input, TEST_DT, mode);
    }
}

static int test_gravity_and_jump(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
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
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 0};
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

static int test_high_speed_wall(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int z = 0; z <= 4; z++) {
        for (int y = 1; y <= 3; y++) {
            ok = ok && check(tc_world_set_block(&world, 3, y, z,
                                                TC_BLOCK_STONE),
                             "high-speed wall fill failed");
        }
    }

    {
        float position[3] = {0.5f, 1.0f, 2.5f};
        tc_player_init(&player, position, pi * 0.5f, 0.0f);
    }

    player.velocity[0] = 120.0f;
    simulate(&player, &world, input, 12);

    ok = ok && check(player.position[0] > 2.65f,
                     "high-speed player did not reach the wall")
      && check(player.position[0] < 2.71f,
               "high-speed player tunnelled through the wall")
      && check(fabsf(player.velocity[0]) < 0.001f,
               "high-speed wall did not clear velocity")
      && check(player.on_ground, "high-speed wall lost ground contact");

    printf("player sweep: high-speed wall stopped at x=%.3f\n",
           player.position[0]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_ceiling_collision(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
    float max_y;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    ok = ok && check(tc_world_set_block(&world, 2, 3, 2,
                                        TC_BLOCK_STONE),
                     "ceiling block failed");

    {
        float position[3] = {2.5f, 1.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    player.velocity[1] = 8.4f;
    max_y = player.position[1];
    for (int frame = 0; frame < 60; frame++) {
        tc_player_update(&player, &world, &input, TEST_DT);
        max_y = fmaxf(max_y, player.position[1]);
    }

    ok = ok && check(max_y <= 1.201f,
                     "player penetrated the ceiling")
      && check(player.position[1] < 1.201f,
               "player remained inside the ceiling")
      && check(player.on_ground, "player did not land after ceiling hit");

    printf("player ceiling: highest feet position %.3f\n", max_y);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_auto_step(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 0};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    ok = ok && check(tc_world_set_block(&world, 3, 1, 2,
                                        TC_BLOCK_STONE),
                     "step block failed");

    {
        float position[3] = {0.5f, 1.0f, 2.5f};
        tc_player_init(&player, position, pi * 0.5f, 0.0f);
    }

    simulate(&player, &world, input, 50);

    ok = ok && check(player.position[0] > 2.9f,
                     "player did not step onto the block")
      && check(fabsf(player.position[1] - 2.0f) < 0.01f,
               "player did not settle on the step")
      && check(player.on_ground, "player lost ground contact after stepping");

    printf("player step: stepped to x=%.3f, y=%.3f\n",
           player.position[0], player.position[1]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_negative_wall(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 0};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int z = 0; z <= 4; z++) {
        for (int y = 1; y <= 3; y++) {
            ok = ok && check(tc_world_set_block(&world, 0, y, z,
                                                TC_BLOCK_STONE),
                             "negative wall fill failed");
        }
    }

    {
        float position[3] = {1.5f, 1.0f, 2.5f};
        tc_player_init(&player, position, -pi * 0.5f, 0.0f);
    }

    simulate(&player, &world, input, 180);

    ok = ok && check(player.position[0] < 1.35f,
                     "player did not move toward the negative wall")
      && check(player.position[0] > 1.29f,
               "player tunnelled through the negative wall")
      && check(player.on_ground, "negative wall lost ground contact");

    printf("player negative wall: stopped at x=%.3f\n",
           player.position[0]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_diagonal_slide(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 0};
    const float pi = 3.14159265358979323846f;
    float start_z;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int z = 0; z <= 4; z++) {
        for (int y = 1; y <= 3; y++) {
            ok = ok && check(tc_world_set_block(&world, 3, y, z,
                                                TC_BLOCK_STONE),
                             "slide wall fill failed");
        }
    }

    {
        float position[3] = {0.5f, 1.0f, 2.5f};
        tc_player_init(&player, position, pi * 0.25f, 0.0f);
    }

    start_z = player.position[2];
    simulate(&player, &world, input, 30);

    ok = ok && check(player.position[0] < 2.71f,
                     "diagonal movement penetrated the wall")
      && check(player.position[2] < start_z - 0.4f,
               "diagonal movement did not slide along the wall")
      && check(player.on_ground, "diagonal slide lost ground contact");

    printf("player slide: x=%.3f, z=%.3f\n",
           player.position[0], player.position[2]);

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
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
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

static int test_game_modes(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    {
        float position[3] = {2.5f, 4.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    simulate_mode(&player, &world, input, 120, TC_GAME_MODE_CREATIVE);
    ok = ok && check(fabsf(player.position[1] - 4.0f) < 0.001f,
                     "creative mode did not hover");

    input.jump = 1;
    simulate_mode(&player, &world, input, 30, TC_GAME_MODE_CREATIVE);
    input.jump = 0;
    ok = ok && check(player.position[1] > 4.5f,
                     "creative mode did not ascend");

    input.descend = 1;
    simulate_mode(&player, &world, input, 60, TC_GAME_MODE_CREATIVE);
    input.descend = 0;
    ok = ok && check(player.position[1] < 4.0f,
                     "creative mode did not descend");

    for (int y = 1; y <= 5; y++) {
        ok = ok && check(tc_world_set_block(&world, 3, y, 2,
                                            TC_BLOCK_STONE),
                         "spectator wall fill failed");
    }

    {
        float position[3] = {2.5f, 2.5f, 2.5f};
        tc_player_init(&player, position, pi * 0.5f, 0.0f);
    }

    input.forward = 1.0f;
    simulate_mode(&player, &world, input, 30, TC_GAME_MODE_SPECTATOR);
    input.forward = 0.0f;
    ok = ok && check(player.position[0] > 4.0f,
                     "spectator mode did not pass through blocks");

    printf("player modes: creative flight and spectator noclip passed\n");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_crouch(void) {
    tc_arena arena;
    tc_world world;
    tc_player crouched;
    tc_player walking;
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 1};
    float eye[3];
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    {
        float position[3] = {2.5f, 1.0f, 2.5f};
        tc_player_init(&crouched, position, 0.0f, 0.0f);
        tc_player_init(&walking, position, 0.0f, 0.0f);
    }

    simulate_mode(&crouched, &world, input, 30, TC_GAME_MODE_SURVIVAL);
    input.crouch = 0;
    simulate_mode(&walking, &world, input, 30, TC_GAME_MODE_SURVIVAL);

    tc_player_get_eye(&crouched, eye);
    ok = ok && check(crouched.crouching, "survival crouch was not applied")
      && check(fabsf(eye[1] - crouched.position[1]
                     - TC_PLAYER_CROUCH_EYE_HEIGHT) < 0.001f,
               "crouched eye height is wrong")
      && check(walking.position[2] < crouched.position[2],
               "crouching did not slow movement");

    input.crouch = 1;
    simulate_mode(&crouched, &world, input, 1, TC_GAME_MODE_SURVIVAL);
    input.crouch = 0;
    simulate_mode(&crouched, &world, input, 1, TC_GAME_MODE_SURVIVAL);
    tc_player_get_eye(&crouched, eye);
    ok = ok && check(!crouched.crouching,
                     "survival crouch did not release in open space")
      && check(fabsf(eye[1] - crouched.position[1]
                     - TC_PLAYER_EYE_HEIGHT) < 0.001f,
               "standing eye height was not restored");

    printf("player crouch: speed, eye height, and release passed\n");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

extern int tc_asm_aabb_collides(const tc_world *world, int min_x, int max_x,
                                int min_y, int max_y, int min_z, int max_z);

static int reference_collides(const tc_world *world, int min_x, int max_x,
                              int min_y, int max_y, int min_z, int max_z) {
    for (int y = min_y; y <= max_y; y++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                if (tc_block_is_solid(tc_world_get_block(world, x, y, z))) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int test_asm_collision_bounds(void) {
    tc_arena arena;
    tc_world world;
    int ok = 1;
    const int y_values[] = {0, 1, 2, 5};

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    for (int z = -20; z <= 20 && ok; z += 3) {
        for (int x = -20; x <= 20 && ok; x += 3) {
            for (size_t index = 0;
                 index < sizeof(y_values) / sizeof(y_values[0]); index++) {
                int y = y_values[index];
                int expected = reference_collides(&world, x, x, y, y, z, z);
                int actual = tc_asm_aabb_collides(&world, x, x, y, y, z, z);

                if (expected != actual) {
                    fprintf(stderr,
                            "asm collision mismatch at %d %d %d: %d != %d\n",
                            x, y, z, actual, expected);
                    ok = 0;
                    break;
                }
            }
        }
    }

    ok = ok && check(tc_asm_aabb_collides(&world, -2, 2, -1, 3, -2, 2)
                     == reference_collides(&world, -2, 2, -1, 3, -2, 2),
                     "asm multi-block bounds mismatch")
      && check(tc_asm_aabb_collides(&world, 2, 5, 2, 4, 2, 5)
               == reference_collides(&world, 2, 5, 2, 4, 2, 5),
               "asm empty bounds mismatch");

    printf("player asm collision: negative and chunk-edge bounds %s\n",
           ok ? "match" : "mismatch");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_survival_fall_damage(void) {
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    {
        float position[3] = {2.5f, 10.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    simulate_mode(&player, &world, input, 240, TC_GAME_MODE_SURVIVAL);

    ok = ok && check(player.survival.health < TC_SURVIVAL_MAX_HEALTH,
                     "survival fall caused no damage")
      && check(player.survival.health > 0.0f,
               "survival fall was instantly lethal");

    printf("player survival: fall damage left %.0f HP at y=%.2f\n",
           player.survival.health, player.position[1]);

    {
        float position[3] = {2.5f, 10.0f, 2.5f};
        tc_player_init(&player, position, 0.0f, 0.0f);
    }

    simulate_mode(&player, &world, input, 240, TC_GAME_MODE_CREATIVE);
    ok = ok && check(fabsf(player.survival.health - TC_SURVIVAL_MAX_HEALTH)
                     < 0.001f,
                     "creative fall changed survival health");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_crouch_edge(void) {
    tc_arena arena;
    tc_world world;
    tc_player crouched;
    tc_player walking;
    tc_player_input input = {1.0f, 0.0f, 0, 0, 0, 1};
    const float pi = 3.14159265358979323846f;
    int ok = 1;

    if (!init_flat_world(&arena, &world)) {
        return 0;
    }

    {
        float position[3] = {4.5f, 1.0f, 2.5f};
        tc_player_init(&crouched, position, pi * 0.5f, 0.0f);
        tc_player_init(&walking, position, pi * 0.5f, 0.0f);
    }

    simulate_mode(&crouched, &world, input, 120, TC_GAME_MODE_SURVIVAL);
    input.crouch = 0;
    simulate_mode(&walking, &world, input, 120, TC_GAME_MODE_SURVIVAL);

    ok = ok && check(crouched.position[0] < 5.35f,
                     "crouching did not stop at the block edge")
      && check(walking.position[0] > crouched.position[0] + 0.5f,
               "uncrouched movement was blocked at the edge");

    printf("player crouch edge: stopped at x=%.3f, walking x=%.3f\n",
           crouched.position[0], walking.position[0]);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_gravity_and_jump();
    ok = ok && test_wall_collision();
    ok = ok && test_high_speed_wall();
    ok = ok && test_ceiling_collision();
    ok = ok && test_auto_step();
    ok = ok && test_negative_wall();
    ok = ok && test_diagonal_slide();
    ok = ok && test_block_intersection();
    ok = ok && test_look_limits();
    ok = ok && test_void_floor();
    ok = ok && test_spawn();
    ok = ok && test_game_modes();
    ok = ok && test_crouch();
    ok = ok && test_survival_fall_damage();
    ok = ok && test_crouch_edge();
    ok = ok && test_asm_collision_bounds();

    if (!ok) {
        return 1;
    }

    printf("test_player: all checks passed\n");
    return 0;
}
