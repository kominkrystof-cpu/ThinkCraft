#include "thinkcraft/platform.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int tc_path_exists(const char *path) {
    return path != NULL && access(path, F_OK) == 0;
}

int tc_executable_dir(char *buffer, size_t size) {
    ssize_t length;
    char *slash;

    if (buffer == NULL || size == 0) {
        return 0;
    }

    length = readlink("/proc/self/exe", buffer, size - 1);
    if (length < 0) {
        return 0;
    }

    buffer[length] = '\0';
    slash = strrchr(buffer, '/');
    if (slash == NULL) {
        snprintf(buffer, size, ".");
        return 1;
    }

    *slash = '\0';
    return 1;
}

char *tc_read_text_file(const char *path, tc_arena *arena) {
    FILE *file;
    long length;
    char *contents;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "tc_read_text_file: %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    contents = tc_arena_alloc(arena, (size_t)length + 1);
    if (contents == NULL) {
        fclose(file);
        return NULL;
    }

    if (fread(contents, 1, (size_t)length, file) != (size_t)length) {
        fclose(file);
        return NULL;
    }

    contents[length] = '\0';
    fclose(file);
    return contents;
}

double tc_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void tc_sleep_ms(double milliseconds) {
    struct timespec ts;
    if (milliseconds <= 0.0) {
        return;
    }

    ts.tv_sec = (time_t)(milliseconds / 1000.0);
    ts.tv_nsec = (long)((milliseconds - (double)ts.tv_sec * 1000.0) * 1000000.0);
    nanosleep(&ts, NULL);
}
