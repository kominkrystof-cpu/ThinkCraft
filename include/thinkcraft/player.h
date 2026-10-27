#ifndef THINKCRAFT_PLAYER_H
#define THINKCRAFT_PLAYER_H

#include "thinkcraft/survival.h"
#include "thinkcraft/world.h"

#define TC_PLAYER_WIDTH 0.6f
#define TC_PLAYER_HEIGHT 1.8f
#define TC_PLAYER_EYE_HEIGHT 1.62f
#define TC_PLAYER_CROUCH_HEIGHT 1.4f
#define TC_PLAYER_CROUCH_EYE_HEIGHT 1.22f

typedef struct {
    float position[3];
    float velocity[3];
    float yaw;
    float pitch;
    int on_ground;
    int crouching;
    tc_survival survival;
} tc_player;

typedef struct {
    float forward;
    float strafe;
    int jump;
    int descend;
    int sprint;
    int crouch;
} tc_player_input;

typedef enum {
    TC_GAME_MODE_SURVIVAL,
    TC_GAME_MODE_CREATIVE,
    TC_GAME_MODE_SPECTATOR,
    TC_GAME_MODE_COUNT
} tc_game_mode;

const char *tc_game_mode_name(tc_game_mode mode);
void tc_player_init(tc_player *player, const float position[3], float yaw,
                    float pitch);
int tc_player_spawn(tc_player *player, const tc_world *world, float x, float z,
                    float yaw, float pitch);
void tc_player_look(tc_player *player, float delta_x, float delta_y,
                    float sensitivity);
void tc_player_update(tc_player *player, const tc_world *world,
                      const tc_player_input *input, float dt);
void tc_player_update_mode(tc_player *player, const tc_world *world,
                           const tc_player_input *input, float dt,
                           tc_game_mode mode);
void tc_player_get_eye(const tc_player *player, float eye[3]);
void tc_player_get_forward(const tc_player *player, float forward[3]);
int tc_player_intersects_block(const tc_player *player, int x, int y, int z);

#endif
