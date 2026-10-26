#ifndef THINKCRAFT_SAVE_H
#define THINKCRAFT_SAVE_H

#include "thinkcraft/player.h"
#include "thinkcraft/world.h"

#include <stddef.h>
#include <stdint.h>

#define TC_SAVE_DEFAULT_PATH "saves/world.tc"
#define TC_SAVE_VERSION 1u
#define TC_SAVE_MAX_EDITS 4000000u

typedef struct {
    uint32_t seed;
    float position[3];
    float yaw;
    float pitch;
    int selected_hotbar;
    tc_world_edit *edits;
    size_t edit_count;
} tc_save_data;

int tc_save_ensure_directory(const char *path);
int tc_save_write(const char *path, const tc_world *world,
                  const tc_player *player, int selected_hotbar);
int tc_save_read(const char *path, tc_save_data *data);
void tc_save_data_shutdown(tc_save_data *data);

#endif
