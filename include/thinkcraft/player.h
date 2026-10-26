#ifndef THINKCRAFT_PLAYER_H
#define THINKCRAFT_PLAYER_H

#include "thinkcraft/world.h"

#define TC_PLAYER_WIDTH 0.6f
#define TC_PLAYER_HEIGHT 1.8f
#define TC_PLAYER_EYE_HEIGHT 1.62f

typedef struct {
    float position[3];
    float velocity[3];
    float yaw;
    float pitch;
    int on_ground;
} tc_player;

typedef struct {
    float forward;
    float strafe;
    int jump;
    int sprint;
} tc_player_input;

void tc_player_init(tc_player *player, const float position[3], float yaw,
                    float pitch);
int tc_player_spawn(tc_player *player, const tc_world *world, float x, float z,
                    float yaw, float pitch);
void tc_player_look(tc_player *player, float delta_x, float delta_y,
                    float sensitivity);
void tc_player_update(tc_player *player, const tc_world *world,
                      const tc_player_input *input, float dt);
void tc_player_get_eye(const tc_player *player, float eye[3]);
void tc_player_get_forward(const tc_player *player, float forward[3]);

#endif
