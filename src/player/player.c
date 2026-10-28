#include "thinkcraft/player.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(tc_world, size) == 8, "world size offset changed");
_Static_assert(offsetof(tc_world, slots) == 32, "world slots offset changed");
_Static_assert(sizeof(tc_world_slot) == 65572, "world slot size changed");
_Static_assert(offsetof(tc_chunk, blocks) == 16, "chunk blocks offset changed");
_Static_assert(offsetof(tc_world_slot, chunk_x) == 65556,
               "world slot chunk x offset changed");
_Static_assert(offsetof(tc_world_slot, chunk_z) == 65560,
               "world slot chunk z offset changed");
_Static_assert(offsetof(tc_world_slot, loaded) == 65564,
               "world slot loaded offset changed");

int tc_asm_aabb_collides(const tc_world *world, int min_x, int max_x,
                         int min_y, int max_y, int min_z, int max_z);

#define TC_PLAYER_WALK_SPEED 4.8f
#define TC_PLAYER_SPRINT_SPEED 7.4f
#define TC_PLAYER_GROUND_ACCEL 42.0f
#define TC_PLAYER_AIR_ACCEL 12.0f
#define TC_PLAYER_GRAVITY 28.0f
#define TC_PLAYER_JUMP_SPEED 8.4f
#define TC_PLAYER_TERMINAL_SPEED 60.0f
#define TC_PLAYER_STEP_HEIGHT 1.02f
#define TC_PLAYER_SUBSTEP 0.125f
#define TC_PLAYER_EPSILON 0.0001f
#define TC_PLAYER_SWEEP_ITERATIONS 12

static float player_height(const tc_player *player) {
    return player->crouching ? TC_PLAYER_CROUCH_HEIGHT : TC_PLAYER_HEIGHT;
}

static int player_collides(const tc_world *world, const tc_player *player,
                           const float position[3]) {
    const float half_width = TC_PLAYER_WIDTH * 0.5f;
    int min_x = (int)floorf(position[0] - half_width);
    int max_x = (int)floorf(position[0] + half_width - TC_PLAYER_EPSILON);
    int min_y = (int)floorf(position[1]);
    int max_y = (int)floorf(position[1] + player_height(player)
                            - TC_PLAYER_EPSILON);
    int min_z = (int)floorf(position[2] - half_width);
    int max_z = (int)floorf(position[2] + half_width - TC_PLAYER_EPSILON);

    return tc_asm_aabb_collides(world, min_x, max_x, min_y, max_y, min_z,
                                max_z);
}

static int player_supported_at(const tc_world *world,
                               const float position[3]) {
    const float half_width = TC_PLAYER_WIDTH * 0.5f;
    int min_x = (int)floorf(position[0] - half_width);
    int max_x = (int)floorf(position[0] + half_width - TC_PLAYER_EPSILON);
    int min_z = (int)floorf(position[2] - half_width);
    int max_z = (int)floorf(position[2] + half_width - TC_PLAYER_EPSILON);
    int support_y = (int)floorf(position[1]) - 1;

    return tc_asm_aabb_collides(world, min_x, max_x, support_y, support_y,
                                min_z, max_z);
}

static void move_axis(tc_player *player, const tc_world *world, int axis,
                      float amount) {
    float start;
    float blocked;
    float safe;

    if (fabsf(amount) <= TC_PLAYER_EPSILON) {
        return;
    }

    if ((axis == 0 || axis == 2) && player->crouching && player->on_ground) {
        float target[3] = {
            player->position[0],
            player->position[1],
            player->position[2]
        };

        target[axis] += amount;
        if (!player_collides(world, player, target)
            && !player_supported_at(world, target)) {
            return;
        }
    }

    start = player->position[axis];
    player->position[axis] += amount;
    if (!player_collides(world, player, player->position)) {
        if (axis == 1 && amount < 0.0f) {
            player->on_ground = 0;
        }
        return;
    }

    if ((axis == 0 || axis == 2) && player->on_ground) {
        float previous_y = player->position[1];

        player->position[1] += TC_PLAYER_STEP_HEIGHT;
        if (!player_collides(world, player, player->position)) {
            player->velocity[1] = 0.0f;
            return;
        }
        player->position[1] = previous_y;
    }

    blocked = player->position[axis];
    safe = start;
    for (int iteration = 0; iteration < TC_PLAYER_SWEEP_ITERATIONS;
         iteration++) {
        float middle = (safe + blocked) * 0.5f;

        player->position[axis] = middle;
        if (player_collides(world, player, player->position)) {
            blocked = middle;
        } else {
            safe = middle;
        }
    }

    player->position[axis] = safe;
    player->velocity[axis] = 0.0f;

    if (axis == 1 && amount < 0.0f) {
        player->on_ground = 1;
    }
}

static void move_player(tc_player *player, const tc_world *world, float dx,
                        float dy, float dz) {
    float largest = fmaxf(fabsf(dx), fmaxf(fabsf(dy), fabsf(dz)));
    int steps = (int)ceilf(largest / TC_PLAYER_SUBSTEP);
    float step_x;
    float step_y;
    float step_z;

    if (steps < 1) {
        steps = 1;
    }

    step_x = dx / (float)steps;
    step_y = dy / (float)steps;
    step_z = dz / (float)steps;

    for (int step = 0; step < steps; step++) {
        move_axis(player, world, 0, step_x);
        move_axis(player, world, 2, step_z);
        move_axis(player, world, 1, step_y);
    }
}

static float approach(float value, float target, float max_delta) {
    if (value < target - max_delta) {
        return value + max_delta;
    }
    if (value > target + max_delta) {
        return value - max_delta;
    }
    return target;
}

void tc_player_init(tc_player *player, const float position[3], float yaw,
                    float pitch) {
    if (player == NULL || position == NULL) {
        return;
    }

    memset(player, 0, sizeof(*player));
    memcpy(player->position, position, sizeof(player->position));
    player->yaw = yaw;
    player->pitch = pitch;
    tc_survival_init(&player->survival);
}

int tc_player_spawn(tc_player *player, const tc_world *world, float x, float z,
                    float yaw, float pitch) {
    int block_x;
    int block_z;

    if (player == NULL || world == NULL) {
        return 0;
    }

    block_x = (int)floorf(x);
    block_z = (int)floorf(z);

    for (int y = TC_CHUNK_SIZE_Y - 1; y >= 0; y--) {
        if (tc_block_is_solid(tc_world_get_block(world, block_x, y,
                                                 block_z))) {
            float position[3] = {
                x,
                (float)y + 1.001f,
                z
            };

            tc_player_init(player, position, yaw, pitch);
            player->on_ground = 0;
            return 1;
        }
    }

    return 0;
}

void tc_player_look(tc_player *player, float delta_x, float delta_y,
                    float sensitivity) {
    const float pi = 3.14159265358979323846f;
    const float max_pitch = pi * 0.5f - 0.01f;
    const float two_pi = pi * 2.0f;

    if (player == NULL) {
        return;
    }

    player->yaw = fmodf(player->yaw + delta_x * sensitivity, two_pi);
    player->pitch -= delta_y * sensitivity;

    if (player->yaw > pi) {
        player->yaw -= two_pi;
    } else if (player->yaw < -pi) {
        player->yaw += two_pi;
    }

    if (player->pitch > max_pitch) {
        player->pitch = max_pitch;
    } else if (player->pitch < -max_pitch) {
        player->pitch = -max_pitch;
    }
}

const char *tc_game_mode_name(tc_game_mode mode) {
    switch (mode) {
    case TC_GAME_MODE_CREATIVE:
        return "CREATIVE";
    case TC_GAME_MODE_SPECTATOR:
        return "SPECTATOR";
    case TC_GAME_MODE_SURVIVAL:
    default:
        return "SURVIVAL";
    }
}

static void player_wish(const tc_player *player, const tc_player_input *input,
                        float wish[2]) {
    float forward = input->forward;
    float strafe = input->strafe;
    float wish_length = sqrtf(forward * forward + strafe * strafe);

    wish[0] = 0.0f;
    wish[1] = 0.0f;

    if (wish_length > 1.0f) {
        forward /= wish_length;
        strafe /= wish_length;
    }

    if (forward != 0.0f || strafe != 0.0f) {
        float sin_yaw = sinf(player->yaw);
        float cos_yaw = cosf(player->yaw);

        wish[0] = sin_yaw * forward + cos_yaw * strafe;
        wish[1] = -cos_yaw * forward + sin_yaw * strafe;
    }
}

void tc_player_update(tc_player *player, const tc_world *world,
                      const tc_player_input *input, float dt) {
    tc_player_update_mode(player, world, input, dt, TC_GAME_MODE_SURVIVAL);
}

void tc_player_update_mode(tc_player *player, const tc_world *world,
                           const tc_player_input *input, float dt,
                           tc_game_mode mode) {
    tc_player_input empty = {0.0f, 0.0f, 0, 0, 0, 0};
    float wish[2];
    float speed;
    float target_x;
    float target_z;
    float acceleration;
    float vertical_target;
    int was_on_ground;
    int jumped = 0;

    if (player == NULL || world == NULL || dt <= 0.0f) {
        return;
    }

    if (input == NULL) {
        input = &empty;
    }

    player_wish(player, input, wish);
    was_on_ground = player->on_ground;

    if (mode == TC_GAME_MODE_SPECTATOR) {
        player->crouching = 0;
        speed = input->sprint ? 14.0f : 9.0f;
        player->velocity[0] = wish[0] * speed;
        player->velocity[2] = wish[1] * speed;
        player->velocity[1] = (float)(input->jump - input->descend) * speed;
        player->position[0] += player->velocity[0] * dt;
        player->position[1] += player->velocity[1] * dt;
        player->position[2] += player->velocity[2] * dt;
        if (player->position[1] < TC_WORLD_FLOOR_Y) {
            player->position[1] = TC_WORLD_FLOOR_Y;
        }
        player->on_ground = 0;
        return;
    }

    if (mode == TC_GAME_MODE_CREATIVE) {
        player->crouching = 0;
        speed = input->sprint ? 10.0f : 7.0f;
        acceleration = 60.0f;
        vertical_target = (float)(input->jump - input->descend) * speed;
    } else {
        if (input->crouch) {
            player->crouching = 1;
        } else if (player->crouching) {
            player->crouching = 0;
            if (player_collides(world, player, player->position)) {
                player->crouching = 1;
            }
        }
        speed = player->crouching
              ? 1.8f
              : (input->sprint ? TC_PLAYER_SPRINT_SPEED
                               : TC_PLAYER_WALK_SPEED);
        acceleration = player->on_ground ? TC_PLAYER_GROUND_ACCEL
                                         : TC_PLAYER_AIR_ACCEL;
        vertical_target = 0.0f;
    }

    target_x = wish[0] * speed;
    target_z = wish[1] * speed;

    player->velocity[0] = approach(player->velocity[0], target_x,
                                   acceleration * dt);
    player->velocity[2] = approach(player->velocity[2], target_z,
                                   acceleration * dt);

    if (mode == TC_GAME_MODE_CREATIVE) {
        player->velocity[1] = approach(player->velocity[1], vertical_target,
                                       acceleration * dt);
    } else if (input->jump && player->on_ground) {
        player->velocity[1] = TC_PLAYER_JUMP_SPEED;
        player->on_ground = 0;
        jumped = 1;
    } else {
        player->velocity[1] -= TC_PLAYER_GRAVITY * dt;
        if (player->velocity[1] < -TC_PLAYER_TERMINAL_SPEED) {
            player->velocity[1] = -TC_PLAYER_TERMINAL_SPEED;
        }
    }

    move_player(player,
                world,
                player->velocity[0] * dt,
                player->velocity[1] * dt,
                player->velocity[2] * dt);

    if (player->position[1] < TC_WORLD_FLOOR_Y) {
        player->position[1] = TC_WORLD_FLOOR_Y;
        player->velocity[1] = 0.0f;
        player->on_ground = 1;
    }

    if (mode == TC_GAME_MODE_SURVIVAL) {
        float horizontal = sqrtf(player->velocity[0] * player->velocity[0]
                                 + player->velocity[2] * player->velocity[2])
                         * dt;

        if (jumped) {
            tc_survival_add_exhaustion(&player->survival, 0.05f);
        }
        if (horizontal > 0.0f && player->on_ground
            && !player->crouching) {
            tc_survival_add_exhaustion(
                &player->survival,
                horizontal * (input->sprint ? 0.1f : 0.01f));
        }

        if (player->on_ground) {
            if (!was_on_ground && player->survival.fall_distance
                                  > TC_SURVIVAL_SAFE_FALL_DISTANCE) {
                tc_survival_apply_fall(&player->survival,
                                       player->survival.fall_distance);
            }
            player->survival.fall_distance = 0.0f;
        } else if (player->velocity[1] < 0.0f) {
            player->survival.fall_distance += -player->velocity[1] * dt;
        }

        tc_survival_tick(&player->survival, dt);
    }
}

void tc_player_get_eye(const tc_player *player, float eye[3]) {
    if (player == NULL || eye == NULL) {
        return;
    }

    eye[0] = player->position[0];
    eye[1] = player->position[1]
           + (player->crouching ? TC_PLAYER_CROUCH_EYE_HEIGHT
                                : TC_PLAYER_EYE_HEIGHT);
    eye[2] = player->position[2];
}

void tc_player_get_forward(const tc_player *player, float forward[3]) {
    float cos_pitch;

    if (player == NULL || forward == NULL) {
        return;
    }

    cos_pitch = cosf(player->pitch);
    forward[0] = sinf(player->yaw) * cos_pitch;
    forward[1] = sinf(player->pitch);
    forward[2] = -cosf(player->yaw) * cos_pitch;
}

int tc_player_intersects_block(const tc_player *player, int x, int y, int z) {
    float half_width;
    float min_x;
    float max_x;
    float min_y;
    float min_z;
    float max_z;

    if (player == NULL) {
        return 0;
    }

    half_width = TC_PLAYER_WIDTH * 0.5f;
    min_x = player->position[0] - half_width;
    max_x = player->position[0] + half_width;
    min_y = player->position[1];
    min_z = player->position[2] - half_width;
    max_z = player->position[2] + half_width;

    return max_x > (float)x && min_x < (float)(x + 1)
        && player->position[1] + player_height(player) > (float)y
        && min_y < (float)(y + 1)
        && max_z > (float)z && min_z < (float)(z + 1);
}
