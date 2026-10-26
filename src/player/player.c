#include "thinkcraft/player.h"

#include <math.h>
#include <string.h>

#define TC_PLAYER_WALK_SPEED 4.8f
#define TC_PLAYER_SPRINT_SPEED 7.4f
#define TC_PLAYER_GROUND_ACCEL 42.0f
#define TC_PLAYER_AIR_ACCEL 12.0f
#define TC_PLAYER_GRAVITY 28.0f
#define TC_PLAYER_JUMP_SPEED 8.4f
#define TC_PLAYER_TERMINAL_SPEED 60.0f
#define TC_PLAYER_STEP_HEIGHT 1.02f
#define TC_PLAYER_SUBSTEP 0.2f
#define TC_PLAYER_EPSILON 0.0001f

static int player_collides(const tc_world *world, const float position[3]) {
    const float half_width = TC_PLAYER_WIDTH * 0.5f;
    int min_x = (int)floorf(position[0] - half_width);
    int max_x = (int)floorf(position[0] + half_width - TC_PLAYER_EPSILON);
    int min_y = (int)floorf(position[1]);
    int max_y = (int)floorf(position[1] + TC_PLAYER_HEIGHT
                            - TC_PLAYER_EPSILON);
    int min_z = (int)floorf(position[2] - half_width);
    int max_z = (int)floorf(position[2] + half_width - TC_PLAYER_EPSILON);

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

static void move_axis(tc_player *player, const tc_world *world, int axis,
                      float amount) {
    if (fabsf(amount) <= TC_PLAYER_EPSILON) {
        return;
    }

    player->position[axis] += amount;
    if (!player_collides(world, player->position)) {
        if (axis == 1 && amount < 0.0f) {
            player->on_ground = 0;
        }
        return;
    }

    if ((axis == 0 || axis == 2) && player->on_ground) {
        float previous_y = player->position[1];

        player->position[1] += TC_PLAYER_STEP_HEIGHT;
        if (!player_collides(world, player->position)) {
            player->velocity[1] = 0.0f;
            return;
        }
        player->position[1] = previous_y;
    }

    if (axis == 0 || axis == 2) {
        float half_width = TC_PLAYER_WIDTH * 0.5f;

        if (amount > 0.0f) {
            player->position[axis] =
                floorf(player->position[axis] + half_width
                       - TC_PLAYER_EPSILON) - half_width;
        } else {
            player->position[axis] =
                floorf(player->position[axis] - half_width) + 1.0f
                + half_width;
        }
    } else if (amount < 0.0f) {
        player->position[1] = floorf(player->position[1]) + 1.0f;
        player->on_ground = 1;
    } else {
        player->position[1] =
            floorf(player->position[1] + TC_PLAYER_HEIGHT
                   - TC_PLAYER_EPSILON) - TC_PLAYER_HEIGHT;
    }

    player->velocity[axis] = 0.0f;
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

void tc_player_update(tc_player *player, const tc_world *world,
                      const tc_player_input *input, float dt) {
    tc_player_input empty = {0.0f, 0.0f, 0, 0};
    float forward;
    float strafe;
    float wish_length;
    float wish_x = 0.0f;
    float wish_z = 0.0f;
    float speed;
    float target_x;
    float target_z;
    float acceleration;

    if (player == NULL || world == NULL || dt <= 0.0f) {
        return;
    }

    if (input == NULL) {
        input = &empty;
    }

    forward = input->forward;
    strafe = input->strafe;
    wish_length = sqrtf(forward * forward + strafe * strafe);
    if (wish_length > 1.0f) {
        forward /= wish_length;
        strafe /= wish_length;
    }

    if (forward != 0.0f || strafe != 0.0f) {
        float sin_yaw = sinf(player->yaw);
        float cos_yaw = cosf(player->yaw);

        wish_x = sin_yaw * forward + cos_yaw * strafe;
        wish_z = -cos_yaw * forward + sin_yaw * strafe;
    }

    speed = input->sprint ? TC_PLAYER_SPRINT_SPEED : TC_PLAYER_WALK_SPEED;
    target_x = wish_x * speed;
    target_z = wish_z * speed;
    acceleration = player->on_ground ? TC_PLAYER_GROUND_ACCEL
                                     : TC_PLAYER_AIR_ACCEL;

    player->velocity[0] = approach(player->velocity[0], target_x,
                                   acceleration * dt);
    player->velocity[2] = approach(player->velocity[2], target_z,
                                   acceleration * dt);

    if (input->jump && player->on_ground) {
        player->velocity[1] = TC_PLAYER_JUMP_SPEED;
        player->on_ground = 0;
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
}

void tc_player_get_eye(const tc_player *player, float eye[3]) {
    if (player == NULL || eye == NULL) {
        return;
    }

    eye[0] = player->position[0];
    eye[1] = player->position[1] + TC_PLAYER_EYE_HEIGHT;
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
        && player->position[1] + TC_PLAYER_HEIGHT > (float)y
        && min_y < (float)(y + 1)
        && max_z > (float)z && min_z < (float)(z + 1);
}
