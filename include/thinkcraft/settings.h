#ifndef THINKCRAFT_SETTINGS_H
#define THINKCRAFT_SETTINGS_H

#include <stddef.h>

#define TC_SETTINGS_MIN_RENDER_DISTANCE 2
#define TC_SETTINGS_MAX_RENDER_DISTANCE 6
#define TC_SETTINGS_DEFAULT_RENDER_DISTANCE 4
#define TC_SETTINGS_DEFAULT_VSYNC 1
#define TC_SETTINGS_DEFAULT_FPS_CAP 0
#define TC_SETTINGS_DEFAULT_RENDER_SCALE 1.0f
#define TC_SETTINGS_DEFAULT_TARGET_FPS 500.0f
#define TC_SETTINGS_DEFAULT_SENSITIVITY 0.0022f
#define TC_SETTINGS_PATH_MAX 4096

typedef struct {
    int render_distance;
    int vsync;
    int fps_cap;
    float render_scale;
    float target_fps;
    float sensitivity;
} tc_settings;

void tc_settings_defaults(tc_settings *settings);
int tc_settings_parse(tc_settings *settings, const char *text);
int tc_settings_load(tc_settings *settings, const char *path);
int tc_settings_save(const tc_settings *settings, const char *path);
int tc_settings_resolve_path(char *buffer, size_t size);

#endif
