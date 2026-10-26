#include "thinkcraft/app.h"
#include "thinkcraft/arena.h"
#include "thinkcraft/hud.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/platform.h"
#include "thinkcraft/player.h"
#include "thinkcraft/raycast.h"
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

typedef struct hotbar_entry {
    uint8_t block;
    const char *name;
    tc_hud_swatch swatch;
} hotbar_entry;

static const hotbar_entry hotbar[] = {
    {TC_BLOCK_GRASS, "grass", {{0.32f, 0.60f, 0.22f}}},
    {TC_BLOCK_DIRT, "dirt", {{0.52f, 0.37f, 0.25f}}},
    {TC_BLOCK_STONE, "stone", {{0.50f, 0.50f, 0.53f}}},
    {TC_BLOCK_SAND, "sand", {{0.86f, 0.80f, 0.61f}}}
};

#define HOTBAR_COUNT ((int)(sizeof(hotbar) / sizeof(hotbar[0])))

static const char *block_name(uint8_t block) {
    for (int index = 0; index < HOTBAR_COUNT; index++) {
        if (hotbar[index].block == block) {
            return hotbar[index].name;
        }
    }
    return block == TC_BLOCK_AIR ? "air" : "unknown";
}

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
    printf("  WASD            Move\n");
    printf("  Mouse           Look around\n");
    printf("  Space           Jump\n");
    printf("  Ctrl            Sprint\n");
    printf("  Escape          Release the mouse, then close the window\n");
    printf("  Left click      Capture the mouse; break the targeted block\n");
    printf("  Right click     Place the selected block\n");
    printf("  1-4             Select grass, dirt, stone, or sand\n");
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

static int rebuild_dirty_chunks(tc_world *world, tc_renderer *renderer,
                                tc_mesh *mesh, tc_render_mesh *render_meshes,
                                tc_mesh_stats *stats, size_t *remeshed_out,
                                size_t *draw_calls_out) {
    tc_mesh_stats totals = {0};
    size_t remeshed = 0;
    size_t draw_calls = 0;

    if (world == NULL || renderer == NULL || mesh == NULL
        || render_meshes == NULL || stats == NULL) {
        return 0;
    }

    for (int index = 0; index < world->size * world->size; index++) {
        tc_world_slot *slot = &world->slots[index];

        if (!slot->loaded) {
            if (render_meshes[index].vao != 0) {
                tc_renderer_destroy_mesh(&render_meshes[index]);
            }
            continue;
        }

        if (slot->dirty) {
            tc_mesh_stats chunk_stats;

            if (!tc_world_build_chunk_mesh(world, slot->chunk_x, slot->chunk_z,
                                           mesh, &chunk_stats)
                || !tc_renderer_upload_mesh(renderer, &render_meshes[index],
                                            mesh)) {
                return 0;
            }

            totals.solid_blocks += chunk_stats.solid_blocks;
            totals.visible_faces += chunk_stats.visible_faces;
            totals.quads += chunk_stats.quads;
            totals.vertices += chunk_stats.vertices;
            totals.indices += chunk_stats.indices;
            totals.scan_ms += chunk_stats.scan_ms;
            totals.build_ms += chunk_stats.build_ms;
            slot->dirty = 0;
            remeshed++;
        }

        if (render_meshes[index].index_count > 0) {
            draw_calls++;
        }
    }

    *stats = totals;
    if (remeshed_out != NULL) {
        *remeshed_out = remeshed;
    }
    if (draw_calls_out != NULL) {
        *draw_calls_out = draw_calls;
    }

    return 1;
}

static int apply_block_edit(tc_world *world, tc_renderer *renderer,
                            tc_mesh *mesh, tc_render_mesh *render_meshes,
                            size_t *draw_calls, int x, int y, int z,
                            uint8_t block) {
    tc_mesh_stats stats;
    size_t remeshed = 0;

    if (!tc_world_set_block(world, x, y, z, block)) {
        return 0;
    }

    return rebuild_dirty_chunks(world, renderer, mesh, render_meshes, &stats,
                                &remeshed, draw_calls);
}

int tc_app_run(int argc, char **argv) {
    const float spawn_x = 8.5f;
    const float spawn_z = 8.5f;
    tc_arena arena;
    tc_world world;
    tc_player player;
    tc_mesh mesh;
    tc_window window;
    tc_renderer renderer;
    tc_render_mesh *render_meshes = NULL;
    app_options options;
    char shader_dir[4096];
    char title[256];
    double start_time = 0.0;
    double last_frame_time = 0.0;
    double last_fps_time = 0.0;
    double elapsed;
    double total_elapsed = 0.0;
    double average_fps = 0.0;
    double mesh_reduction = 0.0;
    size_t slot_count = 0;
    size_t initial_remeshed = 0;
    size_t draw_calls = 0;
    tc_mesh_stats initial_stats;
    tc_hud_swatch hud_swatches[HOTBAR_COUNT];
    long long total_frames = 0;
    int frames_since_report = 0;
    int selected_hotbar = 0;
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

    for (int index = 0; index < HOTBAR_COUNT; index++) {
        hud_swatches[index] = hotbar[index].swatch;
    }

    if (!tc_arena_init(&arena, 16 * 1024 * 1024)) {
        return 1;
    }

    if (!tc_world_init(&world, &arena, TC_WORLD_DEFAULT_RADIUS)) {
        tc_log_error("failed to allocate streaming world");
        goto cleanup;
    }
    world_initialized = 1;
    slot_count = (size_t)world.size * (size_t)world.size;

    tc_world_generate_demo(&world);

    if (!tc_player_spawn(&player, &world, spawn_x, spawn_z, 0.0f, -0.12f)) {
        tc_log_error("could not find a spawn surface");
        goto cleanup;
    }

    tc_log_info("streaming world: %d x %d chunks around the player "
                "(radius %d, %d x %d blocks)",
                world.size, world.size, world.radius,
                world.size * TC_CHUNK_SIZE_X,
                world.size * TC_CHUNK_SIZE_Z);
    tc_log_info("spawn: %.2f %.2f %.2f",
                player.position[0], player.position[1], player.position[2]);

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

    if (slot_count > SIZE_MAX / sizeof(*render_meshes)) {
        tc_log_error("too many world slots");
        goto cleanup;
    }

    render_meshes = tc_arena_alloc(&arena,
                                   slot_count * sizeof(*render_meshes));
    if (render_meshes == NULL) {
        tc_log_error("failed to allocate render meshes");
        goto cleanup;
    }
    memset(render_meshes, 0, slot_count * sizeof(*render_meshes));

    tc_mesh_init(&mesh);
    mesh_initialized = 1;

    if (!rebuild_dirty_chunks(&world, &renderer, &mesh, render_meshes,
                              &initial_stats, &initial_remeshed,
                              &draw_calls)) {
        tc_log_error("failed to build the initial world meshes");
        goto cleanup;
    }

    if (initial_stats.quads > 0) {
        mesh_reduction = (double)initial_stats.visible_faces
                       / (double)initial_stats.quads;
    }

    tc_log_info("world mesh: %zu solid blocks, %zu visible faces -> "
                "%zu quads (%.2fx), %zu vertices, %zu indices",
                initial_stats.solid_blocks, initial_stats.visible_faces,
                initial_stats.quads, mesh_reduction,
                initial_stats.vertices, initial_stats.indices);
    tc_log_info("mesh build: %.2f ms total, %.2f ms average per chunk "
                "(visibility scan %.2f ms)",
                initial_stats.build_ms,
                initial_remeshed > 0
                    ? initial_stats.build_ms / (double)initial_remeshed
                    : 0.0,
                initial_stats.scan_ms);
    tc_log_info("%zu chunks, %zu draw calls", initial_remeshed, draw_calls);

    if (!options.benchmark) {
        tc_log_info("controls: click to capture, WASD move, mouse look, "
                    "Space jump, Ctrl sprint, 1-4 select block, "
                    "left/right break/place, Escape release");
    }

    start_time = tc_window_time();
    last_frame_time = start_time;
    last_fps_time = start_time;

    while (running && !tc_window_should_close(&window)) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        int captured;
        int captured_before;
        int center_chunk_x;
        int center_chunk_z;
        double now;
        double dt;
        float eye[3];
        float forward[3];
        float target[3];
        tc_player_input input = {0.0f, 0.0f, 0, 0};

        tc_window_poll();
        tc_window_framebuffer_size(&window, &framebuffer_width,
                                   &framebuffer_height);

        now = tc_window_time();
        dt = now - last_frame_time;
        last_frame_time = now;
        if (dt < 0.0) {
            dt = 0.0;
        } else if (dt > 0.1) {
            dt = 0.1;
        }

        if (tc_window_key_pressed(&window, TC_KEY_ESCAPE)) {
            if (tc_window_mouse_captured(&window)) {
                tc_window_set_mouse_captured(&window, 0);
            } else {
                running = 0;
            }
        }

        captured = tc_window_mouse_captured(&window);
        captured_before = captured;
        if (!options.benchmark && !captured
            && tc_window_mouse_pressed(&window, TC_MOUSE_LEFT)) {
            tc_window_set_mouse_captured(&window, 1);
            captured = 1;
        }

        if (framebuffer_width <= 0 || framebuffer_height <= 0) {
            tc_window_end_frame(&window);
            tc_sleep_ms(10.0);
            continue;
        }

        if (captured) {
            float mouse_dx;
            float mouse_dy;

            tc_window_consume_mouse_delta(&window, &mouse_dx, &mouse_dy);
            tc_player_look(&player, mouse_dx, mouse_dy, 0.0022f);

            if (tc_window_key_down(&window, TC_KEY_W)) {
                input.forward += 1.0f;
            }
            if (tc_window_key_down(&window, TC_KEY_S)) {
                input.forward -= 1.0f;
            }
            if (tc_window_key_down(&window, TC_KEY_D)) {
                input.strafe += 1.0f;
            }
            if (tc_window_key_down(&window, TC_KEY_A)) {
                input.strafe -= 1.0f;
            }

            input.jump = tc_window_key_down(&window, TC_KEY_SPACE);
            input.sprint = tc_window_key_down(&window, TC_KEY_CTRL);

            for (int index = 0; index < HOTBAR_COUNT; index++) {
                if (tc_window_key_pressed(&window,
                                          (tc_key)(TC_KEY_1 + index))) {
                    if (selected_hotbar != index) {
                        selected_hotbar = index;
                        tc_log_info("selected block: %s",
                                    hotbar[index].name);
                    }
                }
            }
        }

        tc_player_update(&player, &world, &input, (float)dt);

        if (player.position[1] < -32.0f) {
            float position[3] = {spawn_x, 40.0f, spawn_z};

            tc_log_info("player fell out of the world, respawning");
            tc_player_init(&player, position, 0.0f, -0.12f);
        }

        center_chunk_x = tc_math_floor_div((int)floorf(player.position[0]),
                                           TC_CHUNK_SIZE_X);
        center_chunk_z = tc_math_floor_div((int)floorf(player.position[2]),
                                           TC_CHUNK_SIZE_Z);

        if (center_chunk_x != world.center_chunk_x
            || center_chunk_z != world.center_chunk_z) {
            tc_mesh_stats stream_stats;
            size_t stream_remeshed = 0;
            double stream_start = tc_time_now();

            if (!tc_world_update(&world, center_chunk_x, center_chunk_z)
                || !rebuild_dirty_chunks(&world, &renderer, &mesh,
                                         render_meshes, &stream_stats,
                                         &stream_remeshed, &draw_calls)) {
                tc_log_error("chunk streaming failed");
                running = 0;
            } else {
                tc_log_info("streaming to chunk (%d, %d): %zu chunks "
                            "remeshed in %.2f ms",
                            center_chunk_x, center_chunk_z, stream_remeshed,
                            (tc_time_now() - stream_start) * 1000.0);
            }
        }

        tc_player_get_eye(&player, eye);
        tc_player_get_forward(&player, forward);

        if (!options.benchmark && captured && captured_before) {
            int break_clicked = tc_window_mouse_pressed(&window,
                                                        TC_MOUSE_LEFT);
            int place_clicked = tc_window_mouse_pressed(&window,
                                                        TC_MOUSE_RIGHT);

            if (break_clicked || place_clicked) {
                tc_raycast_hit hit;

                if (tc_world_raycast(&world, eye, forward, 6.0f, &hit)) {
                    if (break_clicked) {
                        if (apply_block_edit(&world, &renderer, &mesh,
                                             render_meshes, &draw_calls,
                                             hit.x, hit.y, hit.z,
                                             TC_BLOCK_AIR)) {
                            tc_log_info("broke %s at (%d, %d, %d)",
                                        block_name(hit.block), hit.x, hit.y,
                                        hit.z);
                        }
                    } else if (!tc_player_intersects_block(&player,
                                                           hit.previous_x,
                                                           hit.previous_y,
                                                           hit.previous_z)
                               && tc_world_get_block(&world,
                                                     hit.previous_x,
                                                     hit.previous_y,
                                                     hit.previous_z)
                                  == TC_BLOCK_AIR) {
                        uint8_t block = hotbar[selected_hotbar].block;

                        if (apply_block_edit(&world, &renderer, &mesh,
                                             render_meshes, &draw_calls,
                                             hit.previous_x,
                                             hit.previous_y,
                                             hit.previous_z, block)) {
                            tc_log_info("placed %s at (%d, %d, %d)",
                                        block_name(block), hit.previous_x,
                                        hit.previous_y, hit.previous_z);
                        }
                    }
                }
            }
        }

        target[0] = eye[0] + forward[0];
        target[1] = eye[1] + forward[1];
        target[2] = eye[2] + forward[2];

        tc_renderer_begin_frame(&renderer, eye, target, framebuffer_width,
                                framebuffer_height);
        for (size_t index = 0; index < slot_count; index++) {
            tc_renderer_draw_mesh(&renderer, &render_meshes[index]);
        }
        tc_hud_draw_hotbar(framebuffer_width, framebuffer_height,
                           hud_swatches, HOTBAR_COUNT, selected_hotbar);
        if (captured) {
            tc_hud_draw_crosshair(framebuffer_width, framebuffer_height);
        }
        tc_window_swap(&window);

        frames_since_report++;
        total_frames++;

        now = tc_window_time();
        elapsed = now - last_fps_time;
        if (elapsed >= 1.0) {
            double fps = (double)frames_since_report / elapsed;

            snprintf(title, sizeof(title),
                     "ThinkCraft | chunk (%d, %d) | %s | %.1f FPS | %dx%d",
                     world.center_chunk_x, world.center_chunk_z,
                     hotbar[selected_hotbar].name, fps, framebuffer_width,
                     framebuffer_height);
            tc_window_set_title(&window, title);
            tc_log_info("%.1f FPS", fps);
            frames_since_report = 0;
            last_fps_time = now;
        }

        if (options.benchmark && now - start_time >= options.benchmark_seconds) {
            running = 0;
        }

        tc_window_end_frame(&window);
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
        for (size_t index = 0; index < slot_count; index++) {
            tc_renderer_destroy_mesh(&render_meshes[index]);
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
