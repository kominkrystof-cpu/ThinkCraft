#include "thinkcraft/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_settings: %s\n", message);
    }
    return condition;
}

static int test_defaults(void) {
    tc_settings settings;
    int ok = 1;

    tc_settings_defaults(&settings);
    ok = ok && check(settings.render_distance
                     == TC_SETTINGS_DEFAULT_RENDER_DISTANCE,
                     "wrong default render distance")
      && check(settings.vsync == TC_SETTINGS_DEFAULT_VSYNC,
               "wrong default vsync")
      && check(settings.fps_cap == TC_SETTINGS_DEFAULT_FPS_CAP,
               "wrong default FPS cap")
      && check(settings.render_scale == TC_SETTINGS_DEFAULT_RENDER_SCALE,
               "wrong default render scale")
      && check(settings.target_fps == TC_SETTINGS_DEFAULT_TARGET_FPS,
               "wrong default target FPS")
      && check(settings.sensitivity == TC_SETTINGS_DEFAULT_SENSITIVITY,
               "wrong default sensitivity");

    return ok;
}

static int test_parse(void) {
    tc_settings settings;
    const char *text =
        "# ThinkCraft settings\n"
        "render_distance = 5\n"
        "vsync=0\n"
        "fps_cap = 144\r\n"
        "render_scale=0.75\n"
        "target_fps=240\n"
        "sensitivity=0.0035\n"
        "unknown_key=ignored\n";
    int ok = 1;

    tc_settings_defaults(&settings);
    ok = ok && check(tc_settings_parse(&settings, text),
                     "valid settings were rejected")
      && check(settings.render_distance == 5, "render distance not parsed")
      && check(settings.vsync == 0, "vsync not parsed")
      && check(settings.fps_cap == 144, "FPS cap not parsed")
      && check(settings.render_scale > 0.749f && settings.render_scale < 0.751f,
               "render scale not parsed")
      && check(settings.target_fps > 239.9f && settings.target_fps < 240.1f,
               "target FPS not parsed")
      && check(settings.sensitivity > 0.0034f
               && settings.sensitivity < 0.0036f,
               "sensitivity not parsed");

    ok = ok && check(tc_settings_parse(&settings, "fps_cap=120\n"),
                     "partial settings were rejected")
      && check(settings.fps_cap == 120, "partial setting was not applied")
      && check(settings.render_distance == TC_SETTINGS_DEFAULT_RENDER_DISTANCE,
               "partial settings did not reset other values");

    ok = ok && check(tc_settings_parse(&settings, "render_scale=0\n"),
                     "adaptive render scale was rejected")
      && check(settings.render_scale == 0.0f,
               "adaptive render scale was not applied");

    settings.render_distance = 3;
    ok = ok && check(!tc_settings_parse(&settings, "render_distance=99\n"),
                     "invalid render distance was accepted")
      && check(settings.render_distance == 3,
               "invalid parse modified settings")
      && check(!tc_settings_parse(&settings, "vsync=2\n"),
               "invalid vsync was accepted")
      && check(!tc_settings_parse(&settings, "fps_cap=-1\n"),
               "negative FPS cap was accepted")
      && check(!tc_settings_parse(&settings, "target_fps=0\n"),
               "zero target FPS was accepted")
      && check(!tc_settings_parse(&settings, "sensitivity=1\n"),
               "high sensitivity was accepted")
      && check(!tc_settings_parse(&settings, "malformed line\n"),
               "malformed line was accepted");

    return ok;
}

static int test_round_trip(void) {
    char directory[256];
    char path[512];
    tc_settings settings;
    tc_settings loaded;
    int ok = 1;

    snprintf(directory, sizeof(directory), "/tmp/thinkcraft-settings-%ld",
             (long)getpid());
    snprintf(path, sizeof(path), "%s/nested/config.cfg", directory);

    ok = ok && check(tc_settings_load(&loaded, path) == 0,
                     "missing settings file was not reported");

    tc_settings_defaults(&settings);
    settings.render_distance = 6;
    settings.vsync = 0;
    settings.fps_cap = 60;
    settings.render_scale = 0.0f;
    settings.target_fps = 120.0f;
    settings.sensitivity = 0.0015f;

    ok = ok && check(tc_settings_save(&settings, path),
                     "settings save failed")
      && check(tc_settings_load(&loaded, path) == 1,
               "settings load failed")
      && check(loaded.render_distance == settings.render_distance,
               "render distance round trip failed")
      && check(loaded.vsync == settings.vsync, "vsync round trip failed")
      && check(loaded.fps_cap == settings.fps_cap,
               "FPS cap round trip failed")
      && check(loaded.render_scale == settings.render_scale,
               "render scale round trip failed")
      && check(loaded.target_fps == settings.target_fps,
               "target FPS round trip failed")
      && check(loaded.sensitivity == settings.sensitivity,
               "sensitivity round trip failed");

    unlink(path);
    {
        char nested[512];

        snprintf(nested, sizeof(nested), "%s/nested", directory);
        rmdir(nested);
    }
    rmdir(directory);

    return ok;
}

static int test_path_resolution(void) {
    char path[TC_SETTINGS_PATH_MAX];
    int ok = 1;

    setenv("XDG_CONFIG_HOME", "/tmp/thinkcraft-xdg", 1);
    ok = ok && check(tc_settings_resolve_path(path, sizeof(path)),
                     "XDG settings path was not resolved")
      && check(strcmp(path, "/tmp/thinkcraft-xdg/thinkcraft/config.cfg") == 0,
               "XDG settings path mismatch");

    unsetenv("XDG_CONFIG_HOME");
    setenv("HOME", "/tmp/thinkcraft-home", 1);
    ok = ok && check(tc_settings_resolve_path(path, sizeof(path)),
                     "HOME settings path was not resolved")
      && check(strcmp(path,
                      "/tmp/thinkcraft-home/.config/thinkcraft/config.cfg")
               == 0,
               "HOME settings path mismatch");

    ok = ok && check(!tc_settings_resolve_path(path, 4),
                     "short settings buffer was accepted");

    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_defaults();
    ok = ok && test_parse();
    ok = ok && test_round_trip();
    ok = ok && test_path_resolution();

    if (!ok) {
        return 1;
    }

    printf("test_settings: all checks passed\n");
    return 0;
}
