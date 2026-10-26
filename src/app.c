#include "thinkcraft/app.h"
#include "thinkcraft/arena.h"
#include "thinkcraft/log.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/platform.h"
#include "thinkcraft/renderer.h"
#include "thinkcraft/window.h"
#include "thinkcraft/world.h"

#include <math.h>
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
    tc_world world;
    tc_mesh mesh;
    tc_window window;
    tc_renderer renderer;
    tc_render_mesh *render_meshes = NULL;
    app_options options;
    char shader_dir[4096];
    char title[256];
    double start_time = 0.0;
    double last_fps_time = 0.0;
    double elapsed;
    double total_elapsed = 0.0;
    double average_fps = 0.0;
    double total_build_ms = 0.0;
    double total_scan_ms = 0.0;
    double mesh_reduction = 0.0;
    size_t total_solid = 0;
    size_t total_faces = 0;
    size_t total_quads = 0;
    size_t total_vertices = 0;
    size_t total_indices = 0;
    size_t chunk_count = 0;
    size_t draw_calls = 0;
    long long total_frames = 0;
    int world_width;
    int world_depth;
    float camera_target[3];
    float camera_eye[3];
    float orbit_radius;
    float angle;
    int frames_since_report = 0;
    int running = 1;
    int parse_result;
    int world_initialized = 0;
    int window_created = 0;
    int renderer_initialized = 0;
    int mesh_initialized = 0;
    int exit_code = 1;

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

    if (!tc_world_init(&world, &arena, TC_WORLD_DEFAULT_CHUNKS_X,
                       TC_WORLD_DEFAULT_CHUNKS_Z)) {
        tc_log_error("failed to allocate world");
        goto cleanup;
    }
    world_initialized = 1;

    tc_world_generate_demo(&world);
    chunk_count = (size_t)world.chunks_x * (size_t)world.chunks_z;
    world_width = world.chunks_x * TC_CHUNK_SIZE_X;
    world_depth = world.chunks_z * TC_CHUNK_SIZE_Z;

    tc_log_info("world: %d x %d chunks (%d x %d blocks, height %d)",
                world.chunks_x, world.chunks_z, world_width, world_depth,
                TC_CHUNK_SIZE_Y);

    if (!resolve_shader_dir(shader_dir, sizeof(shader_dir))) {
        tc_log_error("could not locate shaders/basic.vert");
        tc_log_error("run ThinkCraft from the repository or install the shaders directory beside the binary");
        goto cleanup;
    }

    tc_log_info("ThinkCraft starting");
    tc_log_info("shader directory: %s", shader_dir);

    if (!tc_window_create(&window, options.width, options.height, options.vsync,
                          "ThinkCraft")) {
        goto cleanup;
    }
    window_created = 1;

    if (!tc_renderer_init(&renderer, &arena, shader_dir)) {
        tc_log_error("renderer initialization failed");
        goto cleanup;
    }
    renderer_initialized = 1;

    if (chunk_count > SIZE_MAX / sizeof(*render_meshes)) {
        tc_log_error("too many chunks");
        goto cleanup;
    }

    render_meshes = tc_arena_alloc(&arena,
                                   chunk_count * sizeof(*render_meshes));
    if (render_meshes == NULL) {
        tc_log_error("failed to allocate render meshes");
        goto cleanup;
    }
    memset(render_meshes, 0, chunk_count * sizeof(*render_meshes));

    tc_mesh_init(&mesh);
    mesh_initialized = 1;

    for (int chunk_z = 0; chunk_z < world.chunks_z; chunk_z++) {
        for (int chunk_x = 0; chunk_x < world.chunks_x; chunk_x++) {
            size_t chunk_index = (size_t)chunk_z * (size_t)world.chunks_x
                               + (size_t)chunk_x;
            tc_mesh_stats stats;

            if (!tc_world_build_chunk_mesh(&world, chunk_x, chunk_z, &mesh,
                                           &stats)) {
                tc_log_error("greedy meshing failed for chunk (%d, %d)",
                             chunk_x, chunk_z);
                goto cleanup;
            }

            total_solid += stats.solid_blocks;
            total_faces += stats.visible_faces;
            total_quads += stats.quads;
            total_vertices += stats.vertices;
            total_indices += stats.indices;
            total_build_ms += stats.build_ms;
            total_scan_ms += stats.scan_ms;

            if (!tc_renderer_upload_mesh(&renderer, &render_meshes[chunk_index],
                                         &mesh)) {
                tc_log_error("failed to upload chunk (%d, %d) to OpenGL",
                             chunk_x, chunk_z);
                goto cleanup;
            }

            if (render_meshes[chunk_index].index_count > 0) {
                draw_calls++;
            }
        }
    }

    tc_mesh_shutdown(&mesh);
    mesh_initialized = 0;

    if (total_quads > 0) {
        mesh_reduction = (double)total_faces / (double)total_quads;
    }

    tc_log_info("world mesh: %zu solid blocks, %zu visible faces -> "
                "%zu quads (%.2fx), %zu vertices, %zu indices",
                total_solid, total_faces, total_quads, mesh_reduction,
                total_vertices, total_indices);
    tc_log_info("mesh build: %.2f ms total, %.2f ms average per chunk "
                "(visibility scan %.2f ms)",
                total_build_ms, total_build_ms / (double)chunk_count,
                total_scan_ms);
    tc_log_info("%zu chunks, %zu draw calls", chunk_count, draw_calls);

    camera_target[0] = (float)world_width * 0.5f;
    camera_target[1] = 10.0f;
    camera_target[2] = (float)world_depth * 0.5f;
    orbit_radius = (float)(world_width > world_depth
                            ? world_width
                            : world_depth) * 1.4f;

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
        angle = (float)(now - start_time) * 0.10f;
        camera_eye[0] = camera_target[0] + sinf(angle) * orbit_radius;
        camera_eye[1] = camera_target[1] + 42.0f;
        camera_eye[2] = camera_target[2] + cosf(angle) * orbit_radius;

        tc_renderer_begin_frame(&renderer, camera_eye, camera_target,
                                framebuffer_width, framebuffer_height);
        for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
            tc_renderer_draw_mesh(&renderer, &render_meshes[chunk_index]);
        }
        tc_window_swap(&window);

        frames_since_report++;
        total_frames++;

        now = tc_window_time();
        elapsed = now - last_fps_time;
        if (elapsed >= 1.0) {
            fps = (double)frames_since_report / elapsed;
            snprintf(title, sizeof(title),
                     "ThinkCraft | %dx%d chunks | %.1f FPS | %dx%d",
                     world.chunks_x, world.chunks_z, fps,
                     framebuffer_width, framebuffer_height);
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

    if (options.benchmark) {
        tc_log_info("benchmark: %.2f s, %lld frames, %.1f FPS average, "
                    "%.3f ms/frame",
                    total_elapsed, total_frames, average_fps,
                    total_frames > 0
                        ? (total_elapsed * 1000.0 / (double)total_frames)
                        : 0.0);
        tc_log_info("benchmark complete");
    } else {
        tc_log_info("session: %.2f s, %lld frames, %.1f FPS average",
                    total_elapsed, total_frames, average_fps);
    }

    exit_code = 0;

cleanup:
    if (mesh_initialized) {
        tc_mesh_shutdown(&mesh);
    }
    if (render_meshes != NULL) {
        for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
            tc_renderer_destroy_mesh(&render_meshes[chunk_index]);
        }
    }
    if (renderer_initialized) {
        tc_renderer_shutdown(&renderer);
    }
    if (window_created) {
        tc_window_destroy(&window);
    }
    if (world_initialized) {
        tc_world_shutdown(&world);
    }
    tc_arena_shutdown(&arena);
    return exit_code;
}
