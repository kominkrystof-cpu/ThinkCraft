#include "thinkcraft/app.h"
#include "thinkcraft/arena.h"
#include "thinkcraft/log.h"
#include "thinkcraft/platform.h"
#include "thinkcraft/renderer.h"
#include "thinkcraft/window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app_options {
    int width;
    int height;
    int vsync;
    int vsync_set;
    int benchmark;
    double benchmark_seconds;
} app_options;

static void print_help(const char *program) {
    printf("ThinkCraft\n");
    printf("\n");
    printf("Usage: %s [options]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  --width N       Window width in pixels (default 1280)\n");
    printf("  --height N      Window height in pixels (default 720)\n");
    printf("  --vsync         Enable vertical sync\n");
    printf("  --no-vsync      Disable vertical sync (default in benchmark mode)\n");
    printf("  --benchmark     Exit after the benchmark duration\n");
    printf("  --seconds N     Benchmark duration in seconds (default 10)\n");
    printf("  --help          Show this help\n");
    printf("\n");
    printf("Controls:\n");
    printf("  Escape          Close the window\n");
}

static int parse_options(int argc, char **argv, app_options *options) {
    options->width = 1280;
    options->height = 720;
    options->vsync = 1;
    options->vsync_set = 0;
    options->benchmark = 0;
    options->benchmark_seconds = 10.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            options->width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            options->height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--vsync") == 0) {
            options->vsync = 1;
            options->vsync_set = 1;
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            options->vsync = 0;
            options->vsync_set = 1;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            options->benchmark = 1;
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            options->benchmark_seconds = atof(argv[++i]);
        } else {
            tc_log_error("unknown or incomplete option: %s", argv[i]);
            print_help(argv[0]);
            return -1;
        }
    }

    if (options->width <= 0 || options->height <= 0) {
        tc_log_error("window dimensions must be positive");
        return -1;
    }

    if (options->benchmark_seconds <= 0.0) {
        tc_log_error("benchmark duration must be positive");
        return -1;
    }

    if (options->benchmark && !options->vsync_set) {
        options->vsync = 0;
    }

    return 1;
}

static int resolve_shader_dir(char *buffer, size_t size) {
    char current[4096];
    char candidate[4096];
    char *slash;
    size_t dir_length;

    if (size < 9) {
        return 0;
    }

    if (!tc_executable_dir(current, sizeof(current))) {
        snprintf(buffer, size, "shaders");
        return tc_path_exists("shaders/basic.vert");
    }

    for (int depth = 0; depth < 8; depth++) {
        int written = snprintf(candidate, sizeof(candidate),
                               "%s/shaders/basic.vert", current);
        if (written > 0 && (size_t)written < sizeof(candidate)
            && tc_path_exists(candidate)) {
            dir_length = strlen(current);
            if (dir_length > size - 9) {
                dir_length = size - 9;
            }
            memcpy(buffer, current, dir_length);
            memcpy(buffer + dir_length, "/shaders", 9);
            return 1;
        }

        if (strcmp(current, "/") == 0) {
            break;
        }

        slash = strrchr(current, '/');
        if (slash == NULL) {
            break;
        }

        if (slash == current) {
            current[1] = '\0';
        } else {
            *slash = '\0';
        }
    }

    snprintf(buffer, size, "shaders");
    return tc_path_exists("shaders/basic.vert");
}

int tc_app_run(int argc, char **argv) {
    tc_arena arena;
    tc_window window;
    tc_renderer renderer;
    app_options options;
    char shader_dir[4096];
    char title[256];
    double start_time;
    double last_fps_time;
    double elapsed;
    double total_elapsed = 0.0;
    double average_fps = 0.0;
    long long total_frames = 0;
    int frames_since_report = 0;
    int running = 1;
    int parse_result;

    parse_result = parse_options(argc, argv, &options);
    if (parse_result < 0) {
        return 2;
    }
    if (parse_result == 0) {
        return 0;
    }

    if (!tc_arena_init(&arena, 4 * 1024 * 1024)) {
        return 1;
    }

    if (!resolve_shader_dir(shader_dir, sizeof(shader_dir))) {
        tc_log_error("could not locate shaders/basic.vert");
        tc_log_error("run ThinkCraft from the repository or install the shaders directory beside the binary");
        tc_arena_shutdown(&arena);
        return 1;
    }

    tc_log_info("ThinkCraft starting");
    tc_log_info("shader directory: %s", shader_dir);

    if (!tc_window_create(&window, options.width, options.height, options.vsync,
                          "ThinkCraft")) {
        tc_arena_shutdown(&arena);
        return 1;
    }

    if (!tc_renderer_init(&renderer, &arena, shader_dir)) {
        tc_log_error("renderer initialization failed");
        tc_window_destroy(&window);
        tc_arena_shutdown(&arena);
        return 1;
    }

    start_time = tc_window_time();
    last_fps_time = start_time;

    while (running && !tc_window_should_close(&window)) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        double now;
        double fps;

        tc_window_poll();
        tc_window_framebuffer_size(&window, &framebuffer_width, &framebuffer_height);

        if (framebuffer_width <= 0 || framebuffer_height <= 0) {
            tc_sleep_ms(10.0);
            continue;
        }

        now = tc_window_time();
        tc_renderer_frame(&renderer, now - start_time,
                          framebuffer_width, framebuffer_height);
        tc_window_swap(&window);

        frames_since_report++;
        total_frames++;

        now = tc_window_time();
        elapsed = now - last_fps_time;
        if (elapsed >= 1.0) {
            fps = (double)frames_since_report / elapsed;
            snprintf(title, sizeof(title), "ThinkCraft | %.1f FPS | %dx%d",
                     fps, framebuffer_width, framebuffer_height);
            tc_window_set_title(&window, title);
            tc_log_info("%.1f FPS", fps);
            frames_since_report = 0;
            last_fps_time = now;
        }

        if (options.benchmark && now - start_time >= options.benchmark_seconds) {
            running = 0;
        }
    }

    total_elapsed = tc_window_time() - start_time;
    if (total_frames > 0 && total_elapsed > 0.0) {
        average_fps = (double)total_frames / total_elapsed;
    }

    tc_log_info("benchmark: %.2f s, %lld frames, %.1f FPS average, %.3f ms/frame",
                total_elapsed, total_frames, average_fps,
                total_frames > 0 ? (total_elapsed * 1000.0 / (double)total_frames) : 0.0);

    if (options.benchmark) {
        tc_log_info("benchmark complete");
    }

    tc_renderer_shutdown(&renderer);
    tc_window_destroy(&window);
    tc_arena_shutdown(&arena);
    return 0;
}
