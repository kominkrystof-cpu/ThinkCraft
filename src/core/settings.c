#include "thinkcraft/settings.h"
#include "thinkcraft/log.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *trim(char *text) {
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';

    return text;
}

static int parse_int(const char *text, int minimum, int maximum, int *out) {
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < minimum
        || value > maximum) {
        return 0;
    }

    *out = (int)value;
    return 1;
}

static int parse_float(const char *text, float minimum, float maximum,
                       float *out) {
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)
        || value < (double)minimum || value > (double)maximum) {
        return 0;
    }

    *out = (float)value;
    return 1;
}

static int apply_line(tc_settings *settings, char *line) {
    char *cursor = trim(line);
    char *separator;
    char *key;
    char *value;

    if (*cursor == '\0' || *cursor == '#') {
        return 1;
    }

    separator = strchr(cursor, '=');
    if (separator == NULL) {
        return -1;
    }

    *separator = '\0';
    key = trim(cursor);
    value = trim(separator + 1);

    if (strcmp(key, "render_distance") == 0) {
        return parse_int(value, TC_SETTINGS_MIN_RENDER_DISTANCE,
                         TC_SETTINGS_MAX_RENDER_DISTANCE,
                         &settings->render_distance);
    }
    if (strcmp(key, "vsync") == 0) {
        return parse_int(value, 0, 1, &settings->vsync);
    }
    if (strcmp(key, "fps_cap") == 0) {
        return parse_int(value, 0, 10000, &settings->fps_cap);
    }
    if (strcmp(key, "render_scale") == 0) {
        char *end = NULL;
        double parsed;

        errno = 0;
        parsed = strtod(value, &end);
        if (errno != 0 || end == value || *end != '\0' || !isfinite(parsed)
            || (parsed != 0.0 && (parsed < 0.5 || parsed > 1.0))) {
            return 0;
        }
        settings->render_scale = (float)parsed;
        return 1;
    }
    if (strcmp(key, "target_fps") == 0) {
        return parse_float(value, 1.0f, 100000.0f, &settings->target_fps);
    }
    if (strcmp(key, "sensitivity") == 0) {
        return parse_float(value, 0.0001f, 0.1f, &settings->sensitivity);
    }

    return 1;
}

static int ensure_directory(const char *path) {
    char buffer[TC_SETTINGS_PATH_MAX];
    char *slash;
    size_t length;

    if (path == NULL) {
        return 0;
    }

    length = strlen(path);
    if (length == 0 || length >= sizeof(buffer)) {
        return 0;
    }

    memcpy(buffer, path, length + 1);
    slash = strrchr(buffer, '/');
    if (slash == NULL) {
        return 1;
    }

    *slash = '\0';
    for (char *cursor = buffer + 1; *cursor != '\0'; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        *cursor = '\0';
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
            return 0;
        }
        *cursor = '/';
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        return 0;
    }

    return 1;
}

void tc_settings_defaults(tc_settings *settings) {
    if (settings == NULL) {
        return;
    }

    settings->render_distance = TC_SETTINGS_DEFAULT_RENDER_DISTANCE;
    settings->vsync = TC_SETTINGS_DEFAULT_VSYNC;
    settings->fps_cap = TC_SETTINGS_DEFAULT_FPS_CAP;
    settings->render_scale = TC_SETTINGS_DEFAULT_RENDER_SCALE;
    settings->target_fps = TC_SETTINGS_DEFAULT_TARGET_FPS;
    settings->sensitivity = TC_SETTINGS_DEFAULT_SENSITIVITY;
}

int tc_settings_parse(tc_settings *settings, const char *text) {
    tc_settings next;
    char line[512];
    size_t length = 0;

    if (settings == NULL || text == NULL) {
        return 0;
    }

    tc_settings_defaults(&next);

    for (const char *cursor = text;; cursor++) {
        if (*cursor == '\n' || *cursor == '\0') {
            line[length] = '\0';
            if (apply_line(&next, line) <= 0) {
                return 0;
            }
            length = 0;
            if (*cursor == '\0') {
                break;
            }
            continue;
        }

        if (length + 1 >= sizeof(line)) {
            return 0;
        }
        line[length++] = *cursor;
    }

    *settings = next;
    return 1;
}

int tc_settings_load(tc_settings *settings, const char *path) {
    FILE *file;
    long length;
    char *text;

    if (settings == NULL || path == NULL) {
        return -1;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    length = ftell(file);
    if (length < 0 || length > 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    text = malloc((size_t)length + 1);
    if (text == NULL) {
        fclose(file);
        return -1;
    }

    if (fread(text, 1, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return -1;
    }
    text[length] = '\0';
    fclose(file);

    if (!tc_settings_parse(settings, text)) {
        free(text);
        return -1;
    }

    free(text);
    return 1;
}

int tc_settings_save(const tc_settings *settings, const char *path) {
    char temporary[TC_SETTINGS_PATH_MAX];
    FILE *file;
    int written;

    if (settings == NULL || path == NULL || path[0] == '\0'
        || strlen(path) + 5 >= sizeof(temporary)) {
        return 0;
    }

    if (!ensure_directory(path)) {
        tc_log_error("could not create settings directory for %s", path);
        return 0;
    }

    snprintf(temporary, sizeof(temporary), "%s.tmp", path);

    file = fopen(temporary, "wb");
    if (file == NULL) {
        tc_log_error("could not open settings file %s: %s", temporary,
                     strerror(errno));
        return 0;
    }

    written = fprintf(file,
                      "# ThinkCraft settings\n"
                      "render_distance=%d\n"
                      "vsync=%d\n"
                      "fps_cap=%d\n"
                      "render_scale=%.6f\n"
                      "target_fps=%.6f\n"
                      "sensitivity=%.6f\n",
                      settings->render_distance, settings->vsync,
                      settings->fps_cap, settings->render_scale,
                      settings->target_fps, settings->sensitivity) > 0;

    if (fclose(file) != 0) {
        written = 0;
    }

    if (!written) {
        remove(temporary);
        tc_log_error("failed to write settings file %s", path);
        return 0;
    }

    if (rename(temporary, path) != 0) {
        remove(temporary);
        tc_log_error("failed to replace settings file %s: %s", path,
                     strerror(errno));
        return 0;
    }

    return 1;
}

int tc_settings_resolve_path(char *buffer, size_t size) {
    const char *xdg;
    const char *home;
    int written;

    if (buffer == NULL || size == 0) {
        return 0;
    }

    xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && xdg[0] == '/') {
        written = snprintf(buffer, size, "%s/thinkcraft/config.cfg", xdg);
        return written > 0 && (size_t)written < size;
    }

    home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        written = snprintf(buffer, size, "%s/.config/thinkcraft/config.cfg",
                           home);
        return written > 0 && (size_t)written < size;
    }

    return 0;
}
