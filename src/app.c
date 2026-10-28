#include "thinkcraft/app.h"
#include "thinkcraft/arena.h"
#include "thinkcraft/chunk_batch.h"
#include "thinkcraft/entity.h"
#include "thinkcraft/falling.h"
#include "thinkcraft/hud.h"
#include "thinkcraft/inventory.h"
#include "thinkcraft/item.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/platform.h"
#include "thinkcraft/player.h"
#include "thinkcraft/raycast.h"
#include "thinkcraft/renderer.h"
#include "thinkcraft/save.h"
#include "thinkcraft/settings.h"
#include "thinkcraft/terrain.h"
#include "thinkcraft/text.h"
#include "thinkcraft/window.h"
#include "thinkcraft/world.h"

#include <glad/gl.h>

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app_options {
    int width;
    int height;
    int vsync_set;
    int benchmark;
    double benchmark_seconds;
    uint32_t seed;
    char save_path[4096];
    char config_path[TC_SETTINGS_PATH_MAX];
    int config_explicit;
    tc_settings settings;
} app_options;

typedef enum {
    CAMERA_FIRST_PERSON,
    CAMERA_THIRD_PERSON_BACK,
    CAMERA_THIRD_PERSON_FRONT,
    CAMERA_COUNT
} app_camera;

typedef enum {
    APP_STATE_MENU,
    APP_STATE_PLAYING,
    APP_STATE_INVENTORY,
    APP_STATE_PAUSED,
    APP_STATE_DEAD
} app_state;

typedef enum {
    MENU_NEW_WORLD,
    MENU_CONTINUE,
    MENU_SAVE,
    MENU_LOAD,
    MENU_RESUME,
    MENU_QUIT,
    MENU_QUIT_TO_MENU
} menu_action;

typedef struct {
    menu_action action;
    const char *label;
} menu_item;

typedef struct {
    char text[128];
    float color[3];
    double until;
} app_status;

typedef struct {
    double fps;
    double frame_ms;
    float position[3];
    int block_x;
    int block_y;
    int block_z;
    int chunk_x;
    int chunk_z;
    uint32_t seed;
    const char *facing;
    size_t loaded_chunks;
    size_t visible_chunks;
    size_t slot_count;
    int render_distance;
    float render_scale;
    float health;
    float hunger;
    long long rss_bytes;
} app_debug_info;

static const menu_item main_menu_with_continue[] = {
    {MENU_NEW_WORLD, "NEW WORLD"},
    {MENU_CONTINUE, "CONTINUE"},
    {MENU_QUIT, "QUIT"}
};

static const menu_item main_menu_without_continue[] = {
    {MENU_NEW_WORLD, "NEW WORLD"},
    {MENU_QUIT, "QUIT"}
};

static const menu_item pause_menu_items[] = {
    {MENU_RESUME, "RESUME"},
    {MENU_SAVE, "SAVE"},
    {MENU_LOAD, "LOAD"},
    {MENU_QUIT_TO_MENU, "QUIT TO MENU"}
};

#define INV_SLOT_SIZE 40
#define INV_GAP 4
#define INV_COLS 9
#define INV_ROWS 4
#define INV_PADDING 12

static void ui_rect_top(int framebuffer_height, int x, int y, int width,
                        int height, float red, float green, float blue) {
    tc_hud_draw_rect(x, framebuffer_height - y - height, width, height,
                     red, green, blue);
}

static void inventory_geometry(int framebuffer_width, int framebuffer_height,
                               int *panel_x, int *panel_y, int *panel_width,
                               int *panel_height) {
    int width = INV_COLS * INV_SLOT_SIZE + (INV_COLS - 1) * INV_GAP
              + INV_PADDING * 2;
    int height = INV_ROWS * INV_SLOT_SIZE + (INV_ROWS - 1) * INV_GAP
               + INV_PADDING * 2;

    *panel_width = width;
    *panel_height = height;
    *panel_x = (framebuffer_width - width) / 2;
    *panel_y = (framebuffer_height - height) / 2;
}

static int inventory_slot_at(int framebuffer_width, int framebuffer_height,
                             float cursor_x, float cursor_y) {
    int panel_x;
    int panel_y;
    int panel_width;
    int panel_height;
    int local_x;
    int local_y;
    int col;
    int row;

    inventory_geometry(framebuffer_width, framebuffer_height, &panel_x,
                       &panel_y, &panel_width, &panel_height);
    if (cursor_x < (float)panel_x || cursor_y < (float)panel_y
        || cursor_x >= (float)(panel_x + panel_width)
        || cursor_y >= (float)(panel_y + panel_height)) {
        return -1;
    }

    local_x = (int)cursor_x - panel_x - INV_PADDING;
    local_y = (int)cursor_y - panel_y - INV_PADDING;
    col = local_x / (INV_SLOT_SIZE + INV_GAP);
    row = local_y / (INV_SLOT_SIZE + INV_GAP);
    if (col < 0 || col >= INV_COLS || row < 0 || row >= INV_ROWS) {
        return -1;
    }
    if (local_x % (INV_SLOT_SIZE + INV_GAP) >= INV_SLOT_SIZE
        || local_y % (INV_SLOT_SIZE + INV_GAP) >= INV_SLOT_SIZE) {
        return -1;
    }

    if (row == INV_ROWS - 1) {
        return col;
    }
    return TC_INVENTORY_BACKPACK_START + row * INV_COLS + col;
}

static void inventory_draw_slot(int framebuffer_height,
                                int panel_x, int panel_y, int col, int row,
                                const tc_item_stack *stack, int highlighted,
                                int hovered) {
    float color[3];
    int x = panel_x + INV_PADDING + col * (INV_SLOT_SIZE + INV_GAP);
    int y = panel_y + INV_PADDING + row * (INV_SLOT_SIZE + INV_GAP);

    if (hovered) {
        ui_rect_top(framebuffer_height, x - 2, y - 2, INV_SLOT_SIZE + 4,
                    INV_SLOT_SIZE + 4, 0.95f, 0.95f, 0.90f);
    } else if (highlighted) {
        ui_rect_top(framebuffer_height, x - 2, y - 2, INV_SLOT_SIZE + 4,
                    INV_SLOT_SIZE + 4, 0.55f, 0.72f, 0.42f);
    }

    ui_rect_top(framebuffer_height, x, y, INV_SLOT_SIZE, INV_SLOT_SIZE,
                0.16f, 0.17f, 0.19f);

    if (stack->item != TC_ITEM_NONE && stack->count > 0) {
        tc_item_color(stack->item, color);
        ui_rect_top(framebuffer_height, x + 8, y + 8, INV_SLOT_SIZE - 16,
                    INV_SLOT_SIZE - 16, color[0], color[1], color[2]);
    }
}

static void inventory_draw(tc_text_renderer *text, int framebuffer_width,
                           int framebuffer_height, const tc_inventory *inventory,
                           int hovered_slot, const tc_player *player,
                           tc_game_mode mode, float cursor_x, float cursor_y) {
    int panel_x;
    int panel_y;
    int panel_width;
    int panel_height;
    char line[64];
    float color[3];

    inventory_geometry(framebuffer_width, framebuffer_height, &panel_x,
                       &panel_y, &panel_width, &panel_height);

    ui_rect_top(framebuffer_height, panel_x - 4, panel_y - 4,
                panel_width + 8, panel_height + 8,
                0.40f, 0.42f, 0.44f);
    ui_rect_top(framebuffer_height, panel_x, panel_y, panel_width,
                panel_height, 0.10f, 0.11f, 0.13f);

    for (int row = 0; row < INV_ROWS; row++) {
        for (int col = 0; col < INV_COLS; col++) {
            int slot = row == INV_ROWS - 1
                     ? col
                     : TC_INVENTORY_BACKPACK_START + row * INV_COLS + col;

            inventory_draw_slot(framebuffer_height,
                                panel_x, panel_y, col, row,
                                &inventory->slots[slot],
                                row == INV_ROWS - 1
                                    && slot == inventory->selected,
                                slot == hovered_slot);
        }
    }

    if (inventory->cursor.item != TC_ITEM_NONE
        && inventory->cursor.count > 0) {
        tc_item_color(inventory->cursor.item, color);
        ui_rect_top(framebuffer_height, (int)cursor_x - 8, (int)cursor_y - 8,
                    16, 16, color[0], color[1], color[2]);
    }

    tc_text_begin(text, framebuffer_width, framebuffer_height);
    tc_text_draw(text, "INVENTORY", panel_x + INV_PADDING, panel_y - 26, 1.0f,
                 0.95f, 0.95f, 0.90f);
    tc_text_draw(text, tc_game_mode_name(mode),
                 panel_x + panel_width - INV_PADDING
                     - tc_text_width(tc_game_mode_name(mode), 1.0f),
                 panel_y - 26, 1.0f, 0.78f, 0.82f, 0.86f);

    for (int row = 0; row < INV_ROWS; row++) {
        for (int col = 0; col < INV_COLS; col++) {
            int slot = row == INV_ROWS - 1
                     ? col
                     : TC_INVENTORY_BACKPACK_START + row * INV_COLS + col;
            const tc_item_stack *stack = &inventory->slots[slot];
            int x = panel_x + INV_PADDING + col * (INV_SLOT_SIZE + INV_GAP);
            int y = panel_y + INV_PADDING + row * (INV_SLOT_SIZE + INV_GAP);

            if (stack->count > 1) {
                snprintf(line, sizeof(line), "%u", (unsigned)stack->count);
                tc_text_draw(text, line, x + 26, y + 22, 0.8f,
                             0.96f, 0.96f, 0.92f);
            }
        }
    }

    if (hovered_slot >= 0
        && inventory->slots[hovered_slot].item != TC_ITEM_NONE) {
        const tc_item_stack *stack = &inventory->slots[hovered_slot];

        snprintf(line, sizeof(line), "%s X%u", tc_item_name(stack->item),
                 (unsigned)stack->count);
        tc_text_draw(text, line,
                     panel_x + (panel_width - tc_text_width(line, 1.0f)) / 2,
                     panel_y + panel_height + 12, 1.0f,
                     0.92f, 0.92f, 0.88f);
    }

    if (inventory->cursor.item != TC_ITEM_NONE
        && inventory->cursor.count > 0) {
        snprintf(line, sizeof(line), "%u", (unsigned)inventory->cursor.count);
        tc_text_draw(text, line, (int)cursor_x + 10, (int)cursor_y - 10, 1.0f,
                     0.95f, 0.95f, 0.90f);
    }

    snprintf(line, sizeof(line), "XYZ %.1f %.1f %.1f", player->position[0],
             player->position[1], player->position[2]);
    tc_text_draw(text, line, panel_x + INV_PADDING,
                 panel_y + panel_height + 30, 1.0f, 0.70f, 0.74f, 0.78f);
    tc_text_draw(text, "E CLOSE   SHIFT CLICK QUICK MOVE",
                 panel_x + INV_PADDING, panel_y - 44, 1.0f,
                 0.62f, 0.66f, 0.70f);
    tc_text_end(text);
}

static void hotbar_fill_swatches(const tc_inventory *inventory,
                                 tc_hud_swatch *swatches) {
    for (int index = 0; index < TC_ITEM_HOTBAR_COUNT; index++) {
        const tc_item_stack *stack = &inventory->slots[index];

        if (stack->item == TC_ITEM_NONE || stack->count == 0) {
            swatches[index].color[0] = 0.10f;
            swatches[index].color[1] = 0.11f;
            swatches[index].color[2] = 0.12f;
        } else {
            tc_item_color(stack->item, swatches[index].color);
        }
    }
}

static void print_help(const char *program) {
    printf("ThinkCraft\n");
    printf("\n");
    printf("Usage: %s [options]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  --width N       Window width in pixels (default 1920)\n");
    printf("  --height N      Window height in pixels (default 1080)\n");
    printf("  --vsync         Enable vertical sync\n");
    printf("  --no-vsync      Disable vertical sync (default in benchmark mode)\n");
    printf("  --benchmark     Exit after the benchmark duration\n");
    printf("  --seconds N     Benchmark duration in seconds (default 10)\n");
    printf("  --seed N        Terrain seed (default 0x%08x)\n",
           TC_TERRAIN_DEFAULT_SEED);
    printf("  --render-scale N  Internal render scale 0.5-1.0 "
           "(default 1.0, 0 = adaptive)\n");
    printf("  --target-fps N  Adaptive render-scale FPS target (default 500)\n");
    printf("  --render-distance N  Chunk radius 2-6 (default %d)\n",
           TC_SETTINGS_DEFAULT_RENDER_DISTANCE);
    printf("  --fps-cap N     Frame rate cap, 0 = uncapped (default 0)\n");
    printf("  --sensitivity N Mouse sensitivity (default %.4f)\n",
           TC_SETTINGS_DEFAULT_SENSITIVITY);
    printf("  --config PATH   Settings file path\n");
    printf("  --save PATH     Save file path (default %s)\n",
           TC_SAVE_DEFAULT_PATH);
    printf("  --help          Show this help\n");
    printf("\n");
    printf("Controls:\n");
    printf("  WASD            Move\n");
    printf("  Mouse           Look around\n");
    printf("  Space           Jump / fly up\n");
    printf("  Shift           Crouch in Survival; fly down in Creative / Spectator\n");
    printf("  Ctrl            Sprint in Survival; fly down in Creative / Spectator\n");
    printf("  Escape          Release the mouse; press again to pause\n");
    printf("  F3              Toggle the debug overlay\n");
    printf("  F5              Cycle first/third-person camera\n");
    printf("  F6              Quick save\n");
    printf("  F9              Quick load\n");
    printf("  Left click      Capture the mouse; break the targeted block\n");
    printf("  Right click     Place the selected block\n");
    printf("  1-9             Select a hotbar slot\n");
    printf("  Menu            W/S or Up/Down select, Enter activate\n");
}

static int copy_path(char *buffer, size_t size, const char *value,
                     const char *label) {
    if (value == NULL || *value == '\0' || strlen(value) >= size) {
        tc_log_error("invalid %s path: %s", label,
                     value != NULL ? value : "");
        return 0;
    }

    snprintf(buffer, size, "%s", value);
    return 1;
}

static int parse_options(int argc, char **argv, app_options *options) {
    options->width = 1920;
    options->height = 1080;
    options->vsync_set = 0;
    options->benchmark = 0;
    options->benchmark_seconds = 10.0;
    options->seed = TC_TERRAIN_DEFAULT_SEED;
    options->config_path[0] = '\0';
    options->config_explicit = 0;
    tc_settings_defaults(&options->settings);
    snprintf(options->save_path, sizeof(options->save_path), "%s",
             TC_SAVE_DEFAULT_PATH);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc
                || !copy_path(options->config_path,
                              sizeof(options->config_path), argv[i + 1],
                              "config")) {
                return -1;
            }
            options->config_explicit = 1;
            i++;
        }
    }

    if (options->config_path[0] == '\0'
        && !tc_settings_resolve_path(options->config_path,
                                     sizeof(options->config_path))) {
        options->config_path[0] = '\0';
    }

    if (options->config_path[0] != '\0') {
        int loaded = tc_settings_load(&options->settings,
                                      options->config_path);

        if (loaded < 0) {
            if (options->config_explicit) {
                tc_log_error("could not parse settings file %s",
                             options->config_path);
                return -1;
            }
            tc_log_error("could not parse settings file %s, using defaults",
                         options->config_path);
            tc_settings_defaults(&options->settings);
        } else if (loaded == 0 && options->config_explicit) {
            tc_log_info("settings file %s not found, using defaults",
                        options->config_path);
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            return 0;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            options->width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            options->height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--vsync") == 0) {
            options->settings.vsync = 1;
            options->vsync_set = 1;
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            options->settings.vsync = 0;
            options->vsync_set = 1;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            options->benchmark = 1;
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            options->benchmark_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            char *end = NULL;
            unsigned long value;

            errno = 0;
            value = strtoul(argv[++i], &end, 0);
            if (errno != 0 || end == argv[i] || *end != '\0'
                || value > UINT32_MAX) {
                tc_log_error("invalid seed: %s", argv[i]);
                return -1;
            }
            options->seed = (uint32_t)value;
        } else if (strcmp(argv[i], "--render-scale") == 0 && i + 1 < argc) {
            char *end = NULL;
            float value;

            errno = 0;
            value = strtof(argv[++i], &end);
            if (errno != 0 || end == argv[i] || *end != '\0'
                || !isfinite(value)
                || (value != 0.0f && (value < 0.5f || value > 1.0f))) {
                tc_log_error("invalid render scale: %s", argv[i]);
                return -1;
            }
            options->settings.render_scale = value;
        } else if (strcmp(argv[i], "--render-distance") == 0 && i + 1 < argc) {
            char *end = NULL;
            long value;

            errno = 0;
            value = strtol(argv[++i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0'
                || value < TC_SETTINGS_MIN_RENDER_DISTANCE
                || value > TC_SETTINGS_MAX_RENDER_DISTANCE) {
                tc_log_error("invalid render distance: %s", argv[i]);
                return -1;
            }
            options->settings.render_distance = (int)value;
        } else if (strcmp(argv[i], "--fps-cap") == 0 && i + 1 < argc) {
            char *end = NULL;
            long value;

            errno = 0;
            value = strtol(argv[++i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' || value < 0
                || value > 10000) {
                tc_log_error("invalid FPS cap: %s", argv[i]);
                return -1;
            }
            options->settings.fps_cap = (int)value;
        } else if (strcmp(argv[i], "--sensitivity") == 0 && i + 1 < argc) {
            char *end = NULL;
            float value;

            errno = 0;
            value = strtof(argv[++i], &end);
            if (errno != 0 || end == argv[i] || *end != '\0'
                || !isfinite(value) || value < 0.0001f || value > 0.1f) {
                tc_log_error("invalid sensitivity: %s", argv[i]);
                return -1;
            }
            options->settings.sensitivity = value;
        } else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) {
            if (!copy_path(options->save_path, sizeof(options->save_path),
                           argv[++i], "save")) {
                return -1;
            }
        } else if (strcmp(argv[i], "--target-fps") == 0 && i + 1 < argc) {
            char *end = NULL;
            float value;

            errno = 0;
            value = strtof(argv[++i], &end);
            if (errno != 0 || end == argv[i] || *end != '\0'
                || !isfinite(value) || value <= 0.0f) {
                tc_log_error("invalid target FPS: %s", argv[i]);
                return -1;
            }
            options->settings.target_fps = value;
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
        options->settings.vsync = 0;
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

static int rebuild_slot_mesh(tc_world *world, tc_mesh *mesh,
                             tc_chunk_batch *batch, size_t slot_index,
                             tc_world_slot *slot, tc_mesh_stats *totals,
                             size_t *remeshed) {
    tc_mesh_stats chunk_stats;

    if (!tc_world_build_chunk_mesh(world, slot->chunk_x, slot->chunk_z, mesh,
                                   &chunk_stats)
        || !tc_chunk_batch_upload(batch, (int)slot_index, mesh)) {
        return 0;
    }

    totals->solid_blocks += chunk_stats.solid_blocks;
    totals->visible_faces += chunk_stats.visible_faces;
    totals->quads += chunk_stats.quads;
    totals->vertices += chunk_stats.vertices;
    totals->indices += chunk_stats.indices;
    totals->scan_ms += chunk_stats.scan_ms;
    totals->build_ms += chunk_stats.build_ms;
    slot->dirty = 0;
    (*remeshed)++;
    return 1;
}

static int rebuild_dirty_chunks(tc_world *world, tc_mesh *mesh,
                                tc_chunk_batch *batch, size_t max_chunks,
                                double time_budget_ms, int priority_index,
                                tc_mesh_stats *stats, size_t *remeshed_out,
                                size_t *pending_out, size_t *draw_calls_out) {
    tc_mesh_stats totals = {0};
    size_t slot_count;
    size_t remeshed = 0;
    size_t pending = 0;
    size_t draw_calls = 0;
    double start;
    int limit_reached = 0;

    if (world == NULL || mesh == NULL || batch == NULL || stats == NULL) {
        return 0;
    }

    slot_count = (size_t)world->size * (size_t)world->size;
    start = tc_time_now();

    if (priority_index >= 0 && (size_t)priority_index < slot_count) {
        tc_world_slot *slot = &world->slots[priority_index];

        if (!slot->loaded) {
            tc_chunk_batch_clear(batch, priority_index);
        } else if (slot->dirty) {
            if (!rebuild_slot_mesh(world, mesh, batch,
                                   (size_t)priority_index, slot, &totals,
                                   &remeshed)) {
                return 0;
            }
        }
    }

    for (size_t index = 0; index < slot_count; index++) {
        tc_world_slot *slot;

        if ((int)index == priority_index) {
            continue;
        }

        slot = &world->slots[index];
        if (!slot->loaded) {
            tc_chunk_batch_clear(batch, (int)index);
            continue;
        }

        if (!slot->dirty) {
            continue;
        }

        if (!limit_reached && max_chunks != 0 && remeshed >= max_chunks) {
            limit_reached = 1;
        }
        if (!limit_reached && time_budget_ms > 0.0 && remeshed > 0
            && (tc_time_now() - start) * 1000.0 >= time_budget_ms) {
            limit_reached = 1;
        }
        if (limit_reached) {
            pending++;
            continue;
        }

        if (!rebuild_slot_mesh(world, mesh, batch, index, slot, &totals,
                               &remeshed)) {
            return 0;
        }
    }

    for (size_t index = 0; index < slot_count; index++) {
        if (tc_chunk_batch_index_count(batch, (int)index) > 0) {
            draw_calls++;
        }
    }

    *stats = totals;
    if (remeshed_out != NULL) {
        *remeshed_out = remeshed;
    }
    if (pending_out != NULL) {
        *pending_out = pending;
    }
    if (draw_calls_out != NULL) {
        *draw_calls_out = draw_calls;
    }

    return 1;
}

static int find_slot_index(const tc_world *world, int chunk_x, int chunk_z) {
    if (world == NULL) {
        return -1;
    }

    for (int index = 0; index < world->size * world->size; index++) {
        const tc_world_slot *slot = &world->slots[index];

        if (slot->loaded && slot->chunk_x == chunk_x
            && slot->chunk_z == chunk_z) {
            return index;
        }
    }

    return -1;
}

static void extract_frustum_planes(const float mvp[16], float planes[6][4]) {
    planes[0][0] = mvp[3] + mvp[0];
    planes[0][1] = mvp[7] + mvp[4];
    planes[0][2] = mvp[11] + mvp[8];
    planes[0][3] = mvp[15] + mvp[12];

    planes[1][0] = mvp[3] - mvp[0];
    planes[1][1] = mvp[7] - mvp[4];
    planes[1][2] = mvp[11] - mvp[8];
    planes[1][3] = mvp[15] - mvp[12];

    planes[2][0] = mvp[3] + mvp[1];
    planes[2][1] = mvp[7] + mvp[5];
    planes[2][2] = mvp[11] + mvp[9];
    planes[2][3] = mvp[15] + mvp[13];

    planes[3][0] = mvp[3] - mvp[1];
    planes[3][1] = mvp[7] - mvp[5];
    planes[3][2] = mvp[11] - mvp[9];
    planes[3][3] = mvp[15] - mvp[13];

    planes[4][0] = mvp[3] + mvp[2];
    planes[4][1] = mvp[7] + mvp[6];
    planes[4][2] = mvp[11] + mvp[10];
    planes[4][3] = mvp[15] + mvp[14];

    planes[5][0] = mvp[3] - mvp[2];
    planes[5][1] = mvp[7] - mvp[6];
    planes[5][2] = mvp[11] - mvp[10];
    planes[5][3] = mvp[15] - mvp[14];
}

static int chunk_in_frustum(const tc_world *world, size_t index,
                            const float *planes) {
    const tc_world_slot *slot = &world->slots[index];
    float min_x;
    float max_x;
    float min_z;
    float max_z;

    if (!slot->loaded) {
        return 0;
    }

    min_x = (float)slot->chunk.origin_x;
    max_x = min_x + (float)TC_CHUNK_SIZE_X;
    min_z = (float)slot->chunk.origin_z;
    max_z = min_z + (float)TC_CHUNK_SIZE_Z;

    for (int plane = 0; plane < 6; plane++) {
        const float *value = &planes[plane * 4];
        float x = value[0] >= 0.0f ? max_x : min_x;
        float y = value[1] >= 0.0f ? (float)TC_CHUNK_SIZE_Y : 0.0f;
        float z = value[2] >= 0.0f ? max_z : min_z;

        if (value[0] * x + value[1] * y + value[2] * z + value[3] < 0.0f) {
            return 0;
        }
    }

    return 1;
}

static float chunk_distance_sq(const tc_world *world, size_t index,
                               const float eye[3]) {
    const tc_world_slot *slot = &world->slots[index];
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    float dx = 0.0f;
    float dz = 0.0f;

    if (!slot->loaded) {
        return 1.0e30f;
    }

    min_x = (float)slot->chunk.origin_x;
    max_x = min_x + (float)TC_CHUNK_SIZE_X;
    min_z = (float)slot->chunk.origin_z;
    max_z = min_z + (float)TC_CHUNK_SIZE_Z;

    if (eye[0] < min_x) {
        dx = min_x - eye[0];
    } else if (eye[0] > max_x) {
        dx = eye[0] - max_x;
    }
    if (eye[2] < min_z) {
        dz = min_z - eye[2];
    } else if (eye[2] > max_z) {
        dz = eye[2] - max_z;
    }

    return dx * dx + dz * dz;
}

static int apply_block_edit(tc_world *world, tc_mesh *mesh,
                            tc_chunk_batch *batch,
                            tc_falling_blocks *falling, size_t *draw_calls,
                            int x, int y, int z, uint8_t block,
                            double *remesh_ms_out) {
    tc_mesh_stats stats;
    size_t remeshed = 0;
    size_t pending = 0;
    int chunk_x;
    int chunk_z;
    int priority_index;
    double start;

    if (!tc_world_set_block(world, x, y, z, block)) {
        return 0;
    }

    tc_falling_notify_block_changed(falling, world, x, y, z);

    chunk_x = tc_math_floor_div(x, TC_CHUNK_SIZE_X);
    chunk_z = tc_math_floor_div(z, TC_CHUNK_SIZE_Z);
    priority_index = find_slot_index(world, chunk_x, chunk_z);
    start = tc_time_now();

    if (!rebuild_dirty_chunks(world, mesh, batch, 1, 0.0, priority_index,
                              &stats, &remeshed, &pending, draw_calls)) {
        return 0;
    }

    if (remesh_ms_out != NULL) {
        *remesh_ms_out = (tc_time_now() - start) * 1000.0;
    }

    return 1;
}

static void app_status_set(app_status *status, double now, float red,
                           float green, float blue, const char *text) {
    snprintf(status->text, sizeof(status->text), "%s", text);
    status->color[0] = red;
    status->color[1] = green;
    status->color[2] = blue;
    status->until = now + 2.5;
}

static void app_menu_get(app_state state, int has_continue,
                         const menu_item **items, int *count) {
    if (state == APP_STATE_PAUSED) {
        *items = pause_menu_items;
        *count = (int)(sizeof(pause_menu_items) / sizeof(pause_menu_items[0]));
    } else if (has_continue) {
        *items = main_menu_with_continue;
        *count = (int)(sizeof(main_menu_with_continue)
                       / sizeof(main_menu_with_continue[0]));
    } else {
        *items = main_menu_without_continue;
        *count = (int)(sizeof(main_menu_without_continue)
                       / sizeof(main_menu_without_continue[0]));
    }
}

static int rebuild_all_world_meshes(tc_world *world, tc_mesh *mesh,
                                    tc_chunk_batch *batch,
                                    size_t *draw_calls) {
    tc_mesh_stats stats;
    size_t remeshed = 0;
    size_t pending = 0;
    int slot_count = world->size * world->size;

    for (int index = 0; index < slot_count; index++) {
        if (world->slots[index].loaded) {
            world->slots[index].dirty = 1;
        }
    }

    return rebuild_dirty_chunks(world, mesh, batch, SIZE_MAX, 0.0, -1, &stats,
                                &remeshed, &pending, draw_calls);
}

static int start_new_world(tc_world *world, tc_player *player, uint32_t seed,
                           int *selected_hotbar, tc_mesh *mesh,
                           tc_chunk_batch *batch, tc_falling_blocks *falling,
                           size_t *draw_calls) {
    tc_world_set_seed(world, seed);
    if (!tc_world_update(world, 0, 0)) {
        return 0;
    }
    tc_world_generate_terrain(world);
    if (!tc_player_spawn(player, world, 8.5f, 8.5f, 0.0f, -0.12f)) {
        return 0;
    }

    tc_falling_init(falling);
    *selected_hotbar = 0;
    if (!tc_chunk_batch_rebase(batch, (int)floorf(player->position[0]),
                               (int)floorf(player->position[2]))) {
        return 0;
    }
    return rebuild_all_world_meshes(world, mesh, batch, draw_calls);
}

static int save_game(const app_options *options, tc_world *world,
                     tc_falling_blocks *falling, const tc_player *player,
                     const tc_inventory *inventory, tc_game_mode game_mode) {
    /* Falling sand lives in transient entities. Settle it first so a save
     * never drops blocks that happened to be mid-air. */
    tc_falling_settle_all(falling, world);

    if (!tc_save_write(options->save_path, world, player, inventory,
                       game_mode)) {
        return 0;
    }

    tc_log_info("saved world to %s (seed %u, %zu edits)", options->save_path,
                world->seed, world->edit_count);
    return 1;
}

static int load_game(const app_options *options, tc_world *world,
                     tc_player *player, int *selected_hotbar,
                     tc_inventory *inventory, tc_game_mode *game_mode,
                     tc_mesh *mesh, tc_chunk_batch *batch,
                     tc_falling_blocks *falling, size_t *draw_calls) {
    tc_save_data data;
    uint32_t seed;
    size_t edit_count;
    int center_chunk_x;
    int center_chunk_z;

    if (!tc_path_exists(options->save_path)) {
        tc_log_info("no save file at %s", options->save_path);
        return 0;
    }

    if (!tc_save_read(options->save_path, &data)) {
        return 0;
    }

    tc_world_set_seed(world, data.seed);
    tc_falling_init(falling);
    center_chunk_x = tc_math_floor_div((int)floorf(data.position[0]),
                                       TC_CHUNK_SIZE_X);
    center_chunk_z = tc_math_floor_div((int)floorf(data.position[2]),
                                       TC_CHUNK_SIZE_Z);
    if (!tc_world_update(world, center_chunk_x, center_chunk_z)) {
        tc_save_data_shutdown(&data);
        return 0;
    }

    tc_world_generate_terrain(world);

    for (size_t index = 0; index < data.edit_count; index++) {
        const tc_world_edit *edit = &data.edits[index];

        if (!tc_world_add_edit(world, edit->x, edit->y, edit->z,
                               edit->block)) {
            tc_log_error("failed to apply save edit at (%d, %d, %d)",
                         edit->x, edit->y, edit->z);
            tc_save_data_shutdown(&data);
            return 0;
        }
    }

    tc_player_init(player, data.position, data.yaw, data.pitch);
    player->survival.health = data.health;
    player->survival.hunger = data.hunger;
    player->survival.exhaustion = data.exhaustion;
    player->survival.alive = data.health > 0.0f;
    *inventory = data.inventory;
    *game_mode = data.game_mode;
    *selected_hotbar = data.selected_hotbar;
    seed = data.seed;
    edit_count = tc_world_edit_count(world);

    /* Re-check saved edits against the final world so any sand that is no
     * longer supported starts falling instead of hanging in place. */
    for (size_t index = 0; index < data.edit_count; index++) {
        const tc_world_edit *edit = &data.edits[index];

        tc_falling_notify_block_changed(falling, world, edit->x, edit->y,
                                        edit->z);
    }
    tc_save_data_shutdown(&data);

    if (!tc_chunk_batch_rebase(batch, (int)floorf(player->position[0]),
                               (int)floorf(player->position[2]))) {
        return 0;
    }

    if (!rebuild_all_world_meshes(world, mesh, batch, draw_calls)) {
        return 0;
    }

    tc_log_info("loaded world from %s (seed %u, %zu edits)", options->save_path,
                seed, edit_count);
    return 1;
}

static void draw_text_centered(tc_text_renderer *text, const char *string,
                               int framebuffer_width, int y, float scale,
                               float red, float green, float blue) {
    int width = tc_text_width(string, scale);

    tc_text_draw(text, string, (framebuffer_width - width) / 2, y, scale, red,
                 green, blue);
}

static const char *facing_name(const float forward[3]) {
    float x = fabsf(forward[0]);
    float z = fabsf(forward[2]);

    if (x > z) {
        return forward[0] >= 0.0f ? "EAST +X" : "WEST -X";
    }

    return forward[2] >= 0.0f ? "SOUTH +Z" : "NORTH -Z";
}

static const char *camera_mode_name(app_camera mode) {
    switch (mode) {
        case CAMERA_THIRD_PERSON_BACK:
            return "THIRD PERSON BACK";
        case CAMERA_THIRD_PERSON_FRONT:
            return "THIRD PERSON FRONT";
        case CAMERA_FIRST_PERSON:
        default:
            return "FIRST PERSON";
    }
}

static void compute_camera(const tc_world *world, const tc_player *player,
                           app_camera mode, float eye[3],
                           float target[3]) {
    float player_eye[3];
    float forward[3];
    float focus[3];

    tc_player_get_eye(player, player_eye);
    tc_player_get_forward(player, forward);

    if (mode == CAMERA_FIRST_PERSON) {
        memcpy(eye, player_eye, sizeof(player_eye));
        target[0] = eye[0] + forward[0];
        target[1] = eye[1] + forward[1];
        target[2] = eye[2] + forward[2];
        return;
    }

    focus[0] = player->position[0];
    focus[1] = player->position[1] + (player->crouching ? 1.02f : 1.34f);
    focus[2] = player->position[2];

    float distance = 3.6f;
    float direction[3] = {forward[0], forward[1], forward[2]};

    if (mode == CAMERA_THIRD_PERSON_BACK) {
        direction[0] = -direction[0];
        direction[1] = -direction[1];
        direction[2] = -direction[2];
    }

    {
        tc_raycast_hit hit;

        if (tc_world_raycast(world, focus, direction, distance, &hit)) {
            distance = hit.distance - 0.18f;
            if (distance < 0.35f) {
                distance = 0.35f;
            }
        }
    }

    eye[0] = focus[0] + direction[0] * distance;
    eye[1] = focus[1] + direction[1] * distance;
    eye[2] = focus[2] + direction[2] * distance;
    memcpy(target, focus, sizeof(focus));
}

static void draw_debug_overlay(tc_text_renderer *text, int framebuffer_width,
                               int framebuffer_height,
                               const app_debug_info *info) {
    char lines[12][128];
    const float scale = 1.5f;
    const int line_height = 15;
    int count = 0;
    int panel_x = 8;
    int panel_width;
    int panel_height;

    snprintf(lines[count++], sizeof(lines[0]), "THINKCRAFT DEBUG");
    snprintf(lines[count++], sizeof(lines[0]), "%.1f FPS  %.2f MS",
             info->fps, info->frame_ms);
    snprintf(lines[count++], sizeof(lines[0]), "XYZ %.2f %.2f %.2f",
             info->position[0], info->position[1], info->position[2]);
    snprintf(lines[count++], sizeof(lines[0]), "BLOCK %d %d %d",
             info->block_x, info->block_y, info->block_z);
    snprintf(lines[count++], sizeof(lines[0]), "CHUNK %d %d  SEED %08X",
             info->chunk_x, info->chunk_z, (unsigned int)info->seed);
    snprintf(lines[count++], sizeof(lines[0]), "FACING %s", info->facing);
    snprintf(lines[count++], sizeof(lines[0]), "CHUNKS %zu/%zu  VISIBLE %zu",
             info->loaded_chunks, info->slot_count, info->visible_chunks);
    snprintf(lines[count++], sizeof(lines[0]), "RENDER %d  SCALE %.2f",
             info->render_distance, info->render_scale);
    snprintf(lines[count++], sizeof(lines[0]), "HP %.0f  FOOD %.0f",
             info->health, info->hunger);
    snprintf(lines[count++], sizeof(lines[0]), "RAM %.1f MB",
             (double)info->rss_bytes / (1024.0 * 1024.0));
    snprintf(lines[count++], sizeof(lines[0]), "VRAM N/A");

    panel_width = 430;
    if (panel_width > framebuffer_width - 16) {
        panel_width = framebuffer_width - 16;
    }
    panel_height = count * line_height + 16;

    tc_hud_draw_rect(panel_x, framebuffer_height - panel_height - 8,
                     panel_width, panel_height, 0.03f, 0.04f, 0.06f);

    tc_text_begin(text, framebuffer_width, framebuffer_height);
    for (int index = 0; index < count; index++) {
        float red = index == 0 ? 0.96f : 0.82f;
        float green = index == 0 ? 0.90f : 0.90f;
        float blue = index == 0 ? 0.62f : 0.86f;

        tc_text_draw(text, lines[index], panel_x + 8, 16 + index * line_height,
                     scale, red, green, blue);
    }
    tc_text_end(text);
}

static void draw_menu(tc_text_renderer *text, int framebuffer_width,
                      int framebuffer_height, const menu_item *items,
                      int count, int selected, const char *title,
                      const app_status *status, double now, int paused) {
    const float item_scale = 2.0f;
    const int line_height = 28;
    int panel_x = 0;
    int panel_y = 0;
    int panel_width = framebuffer_width;
    int panel_height = framebuffer_height;
    int title_y;
    int items_y;
    int footer_y;
    int status_y;

    if (paused) {
        panel_width = framebuffer_width - 80;
        if (panel_width > 560) {
            panel_width = 560;
        }
        if (panel_width < 240) {
            panel_width = framebuffer_width - 24;
        }
        panel_height = framebuffer_height - 160;
        if (panel_height > 380) {
            panel_height = 380;
        }
        if (panel_height < 240) {
            panel_height = framebuffer_height - 48;
        }
        panel_x = (framebuffer_width - panel_width) / 2;
        panel_y = (framebuffer_height - panel_height) / 2;

        tc_hud_draw_rect(panel_x - 4, panel_y - 4, panel_width + 8,
                         panel_height + 8, 0.52f, 0.56f, 0.61f);
        tc_hud_draw_rect(panel_x, panel_y, panel_width, panel_height,
                         0.055f, 0.065f, 0.085f);
    } else {
        tc_hud_draw_rect(0, 0, framebuffer_width, framebuffer_height,
                         0.035f, 0.045f, 0.060f);
    }

    title_y = paused ? panel_y + 30 : framebuffer_height / 5;
    items_y = paused ? panel_y + 112
                     : framebuffer_height / 2 - (count * line_height) / 2;
    footer_y = paused ? panel_y + panel_height - 62
                      : framebuffer_height - 78;
    status_y = paused ? panel_y + panel_height - 28
                      : framebuffer_height - 38;

    tc_text_begin(text, framebuffer_width, framebuffer_height);

    draw_text_centered(text, title, framebuffer_width, title_y,
                       paused ? 3.0f : 4.0f, 0.96f, 0.90f, 0.62f);
    if (!paused) {
        draw_text_centered(text, "A TINY VOXEL ENGINE", framebuffer_width,
                           title_y + 52, 1.5f, 0.55f, 0.62f, 0.70f);
    }

    for (int index = 0; index < count; index++) {
        char line[96];
        int y = items_y + index * line_height;
        float red = index == selected ? 1.0f : 0.78f;
        float green = index == selected ? 0.90f : 0.81f;
        float blue = index == selected ? 0.45f : 0.85f;

        snprintf(line, sizeof(line), "%c %s", index == selected ? '>' : ' ',
                 items[index].label);
        draw_text_centered(text, line, framebuffer_width, y, item_scale,
                           red, green, blue);
    }

    if (paused) {
        draw_text_centered(text, "ESC RESUME   F6 SAVE   F9 LOAD",
                           framebuffer_width, footer_y, 1.5f, 0.48f, 0.53f,
                           0.60f);
    } else {
        draw_text_centered(text, "W/S OR UP/DOWN SELECT", framebuffer_width,
                           footer_y, 1.5f, 0.48f, 0.53f, 0.60f);
        draw_text_centered(text, "ENTER ACTIVATE", framebuffer_width,
                           footer_y + 20, 1.5f, 0.48f, 0.53f, 0.60f);
    }

    if (status->until > now && status->text[0] != '\0') {
        draw_text_centered(text, status->text, framebuffer_width, status_y,
                           2.0f, status->color[0], status->color[1],
                           status->color[2]);
    }

    tc_text_end(text);
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
    tc_entity_renderer entities;
    tc_text_renderer text;
    tc_chunk_batch chunk_batch;
    tc_falling_blocks falling;
    size_t *draw_order = NULL;
    float *draw_distances = NULL;
    app_options options;
    app_status status;
    app_state state;
    char shader_dir[4096];
    char title[256];
    double start_time = 0.0;
    double last_frame_time = 0.0;
    double last_fps_time = 0.0;
    double display_fps = 0.0;
    double elapsed;
    double total_elapsed = 0.0;
    double average_fps = 0.0;
    size_t slot_count = 0;
    size_t initial_remeshed = 0;
    size_t initial_pending = 0;
    size_t draw_calls = 0;
    tc_mesh_stats initial_stats;
    tc_hud_swatch hud_swatches[TC_ITEM_HOTBAR_COUNT];
    tc_inventory inventory;
    tc_game_mode game_mode = TC_GAME_MODE_SURVIVAL;
    int mining_active = 0;
    int mining_x = 0;
    int mining_y = 0;
    int mining_z = 0;
    float mining_progress = 0.0f;
    int inventory_hovered = -1;
    long long total_frames = 0;
    int frames_since_report = 0;
    int selected_hotbar = 0;
    app_camera camera_mode = CAMERA_FIRST_PERSON;
    float walk_phase = 0.0f;
    float walk_amount = 0.0f;
    int hand_swinging = 0;
    float hand_swing = 0.0f;
    int running = 1;
    int show_debug = 0;
    long long rss_bytes = 0;
    int main_menu_has_continue = 0;
    int menu_selection = 0;
    int parse_result;
    int world_initialized = 0;
    int window_created = 0;
    int renderer_initialized = 0;
    int entities_initialized = 0;
    int text_initialized = 0;
    int hud_initialized = 0;
    int mesh_initialized = 0;
    int chunk_batch_initialized = 0;
    int exit_code = 1;
    float render_scale = 1.0f;
    float fog_start = TC_RENDER_FOG_START;
    float fog_end = TC_RENDER_FOG_END;

    parse_result = parse_options(argc, argv, &options);
    if (parse_result < 0) {
        return 2;
    }
    if (parse_result == 0) {
        return 0;
    }

    /* World load and streaming rebuild many chunks per frame, so skip the
     * solid-block and visible-face scan passes there. quads, vertices,
     * indices and build_ms are still filled in by the mesh builder itself. */
    tc_mesh_stats_scan_enabled = 0;

    memset(&status, 0, sizeof(status));
    state = options.benchmark ? APP_STATE_PLAYING : APP_STATE_MENU;
    main_menu_has_continue = tc_path_exists(options.save_path);

    if (!tc_arena_init(&arena, 32 * 1024 * 1024)) {
        return 1;
    }

    if (!tc_world_init(&world, &arena, TC_WORLD_MAX_RADIUS)) {
        tc_log_error("failed to allocate streaming world");
        goto cleanup;
    }
    world_initialized = 1;

    if (!tc_world_set_radius(&world, options.settings.render_distance)) {
        tc_log_error("failed to set render distance");
        goto cleanup;
    }
    slot_count = (size_t)world.size * (size_t)world.size;

    tc_world_set_seed(&world, options.seed);
    tc_world_generate_terrain(&world);

    if (!tc_player_spawn(&player, &world, spawn_x, spawn_z, 0.0f, -0.12f)) {
        tc_log_error("could not find a spawn surface");
        goto cleanup;
    }

    tc_inventory_init_starter(&inventory);

    tc_log_info("world seed: %u", world.seed);
    tc_log_info("streaming world: %d x %d active chunks around the player "
                "(radius %d, %d x %d capacity)",
                world.radius * 2 + 1, world.radius * 2 + 1, world.radius,
                world.size, world.size);
    tc_log_info("settings file: %s",
                options.config_path[0] != '\0' ? options.config_path
                                               : "(disabled)");
    tc_log_info("spawn: %.2f %.2f %.2f",
                player.position[0], player.position[1], player.position[2]);

    if (!resolve_shader_dir(shader_dir, sizeof(shader_dir))) {
        tc_log_error("could not locate shaders/basic.vert");
        tc_log_error("run ThinkCraft from the repository or install the shaders directory beside the binary");
        goto cleanup;
    }

    tc_log_info("ThinkCraft starting");
    tc_log_info("shader directory: %s", shader_dir);
    tc_log_info("save file: %s (%s)", options.save_path,
                main_menu_has_continue ? "continue available" : "new world");

    if (!tc_window_create(&window, options.width, options.height,
                          options.settings.vsync, "ThinkCraft")) {
        goto cleanup;
    }
    window_created = 1;

    if (!tc_renderer_init(&renderer, &arena, shader_dir)) {
        tc_log_error("renderer initialization failed");
        goto cleanup;
    }
    renderer_initialized = 1;

    if (!tc_text_init(&text, &arena, shader_dir)) {
        tc_log_error("text renderer initialization failed");
        goto cleanup;
    }
    text_initialized = 1;

    if (!tc_hud_init(&arena, shader_dir)) {
        tc_log_error("hud initialization failed");
        goto cleanup;
    }
    hud_initialized = 1;

    if (!tc_entity_renderer_init(&entities, &renderer)) {
        tc_log_error("entity renderer initialization failed");
        goto cleanup;
    }
    entities_initialized = 1;

    tc_renderer_fog_for_radius(world.radius, &fog_start, &fog_end);
    tc_renderer_set_fog(&renderer, fog_start, fog_end);

    if (options.settings.render_scale > 0.0f) {
        render_scale = options.settings.render_scale;
        tc_renderer_set_render_scale(&renderer, render_scale);
        tc_log_info("render scale: %.2fx", render_scale);
    } else if (options.settings.vsync) {
        tc_log_info("render scale: native (adaptive scaling needs vsync off)");
    } else {
        tc_log_info("render scale: adaptive, %.1f FPS target",
                    options.settings.target_fps);
    }

    tc_log_info("settings: render distance %d, vsync %s, FPS cap %d, "
                "sensitivity %.4f",
                options.settings.render_distance,
                options.settings.vsync ? "on" : "off",
                options.settings.fps_cap, options.settings.sensitivity);

    if (slot_count > TC_CHUNK_BATCH_MAX_SLOTS) {
        tc_log_error("too many world slots for the chunk batcher");
        goto cleanup;
    }

    if (!tc_chunk_batch_init(&chunk_batch, (int)slot_count)) {
        tc_log_error("failed to initialize the chunk batcher");
        goto cleanup;
    }
    chunk_batch_initialized = 1;
    tc_falling_init(&falling);

    if (slot_count > SIZE_MAX / sizeof(*draw_order)) {
        tc_log_error("too many draw order entries");
        goto cleanup;
    }

    draw_order = tc_arena_alloc(&arena, slot_count * sizeof(*draw_order));
    if (draw_order == NULL) {
        tc_log_error("failed to allocate draw order");
        goto cleanup;
    }

    if (slot_count > SIZE_MAX / sizeof(*draw_distances)) {
        tc_log_error("too many draw distances");
        goto cleanup;
    }

    draw_distances = tc_arena_alloc(&arena,
                                    slot_count * sizeof(*draw_distances));
    if (draw_distances == NULL) {
        tc_log_error("failed to allocate draw distances");
        goto cleanup;
    }

    tc_mesh_init(&mesh);
    mesh_initialized = 1;

    if (!rebuild_dirty_chunks(&world, &mesh, &chunk_batch,
                              SIZE_MAX, 0.0, -1, &initial_stats,
                              &initial_remeshed, &initial_pending,
                              &draw_calls)) {
        tc_log_error("failed to build the initial world meshes");
        goto cleanup;
    }

    /* The scan passes are off, so the solid-block total comes from the running
     * count the chunks maintain instead of a re-count of every block. */
    {
        size_t solid_blocks = 0;

        for (size_t index = 0; index < slot_count; index++) {
            if (world.slots[index].loaded) {
                solid_blocks += world.slots[index].chunk.solid_count;
            }
        }

        tc_log_info("world mesh: %zu solid blocks, %zu quads, %zu vertices, "
                    "%zu indices",
                    solid_blocks, initial_stats.quads, initial_stats.vertices,
                    initial_stats.indices);
    }

    tc_log_info("mesh build: %.2f ms total, %.2f ms average per chunk",
                initial_stats.build_ms,
                initial_remeshed > 0
                    ? initial_stats.build_ms / (double)initial_remeshed
                    : 0.0);
    tc_log_info("%zu chunks uploaded, %zu non-empty slots, batched into 1 "
                "multi-draw", initial_remeshed, draw_calls);

    if (!options.benchmark) {
        tc_log_info("controls: WASD move, mouse look, Space jump/fly up, "
                    "Ctrl sprint, Shift crouch, Shift or Ctrl fly down, "
                    "1-9 select slot, E inventory, F4 game mode, "
                    "hold left to mine, right place, Escape pause, F5 camera, F6 save, F9 load");
        if (state == APP_STATE_MENU) {
            tc_log_info("main menu active: W/S or Up/Down select, Enter activate");
        }
    }

    start_time = tc_window_time();
    last_frame_time = start_time;
    last_fps_time = start_time;

    while (running && !tc_window_should_close(&window)) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        int captured;
        int captured_before;
        int swing_continue = 0;
        int center_chunk_x;
        int center_chunk_z;
        int edited = 0;
        size_t visible_chunks = 0;
        double now;
        double dt;
        double frame_start;
        float eye[3];
        float forward[3];
        float target[3];
        float camera_mvp[16];
        uint16_t held_item;
        float frustum_planes[6][4];
        float cursor_x = 0.0f;
        float cursor_y = 0.0f;
        tc_player_input input = {0.0f, 0.0f, 0, 0, 0, 0};
        const menu_item *menu_items = NULL;
        int menu_count = 0;

        tc_window_poll();
        tc_window_framebuffer_size(&window, &framebuffer_width,
                                   &framebuffer_height);

        frame_start = tc_window_time();
        now = frame_start;
        dt = now - last_frame_time;
        last_frame_time = now;
        if (dt < 0.0) {
            dt = 0.0;
        } else if (dt > 0.1) {
            dt = 0.1;
        }

        if (framebuffer_width <= 0 || framebuffer_height <= 0) {
            tc_window_end_frame(&window);
            tc_sleep_ms(10.0);
            continue;
        }

        if (tc_window_key_pressed(&window, TC_KEY_ESCAPE)) {
            if (state == APP_STATE_INVENTORY) {
                state = APP_STATE_PLAYING;
                tc_window_set_mouse_captured(&window, 1);
                mining_active = 0;
            } else if (state == APP_STATE_PLAYING) {
                if (tc_window_mouse_captured(&window)) {
                    tc_window_set_mouse_captured(&window, 0);
                } else {
                    state = APP_STATE_PAUSED;
                    menu_selection = 0;
                    tc_window_set_mouse_captured(&window, 0);
                    tc_log_info("paused");
                }
            } else if (state == APP_STATE_PAUSED) {
                state = APP_STATE_PLAYING;
                tc_log_info("resumed");
            } else {
                running = 0;
            }
        }

        if (state == APP_STATE_DEAD
            && (tc_window_key_pressed(&window, TC_KEY_SPACE)
                || tc_window_key_pressed(&window, TC_KEY_ENTER))) {
            float position[3] = {spawn_x, 40.0f, spawn_z};

            if (!tc_player_spawn(&player, &world, spawn_x, spawn_z,
                                 0.0f, -0.12f)) {
                tc_player_init(&player, position, 0.0f, -0.12f);
            }
            state = APP_STATE_PLAYING;
            tc_window_set_mouse_captured(&window, 1);
            tc_log_info("respawned after death");
        }

        if (tc_window_key_pressed(&window, TC_KEY_E)
            && state != APP_STATE_MENU && state != APP_STATE_PAUSED
            && state != APP_STATE_DEAD && !options.benchmark) {
            if (state == APP_STATE_PLAYING
                && tc_window_mouse_captured(&window)) {
                state = APP_STATE_INVENTORY;
                tc_window_set_mouse_captured(&window, 0);
                mining_active = 0;
                tc_log_info("inventory opened");
            } else if (state == APP_STATE_INVENTORY) {
                state = APP_STATE_PLAYING;
                tc_window_set_mouse_captured(&window, 1);
                mining_active = 0;
                tc_log_info("inventory closed");
            }
        }

        if (tc_window_key_pressed(&window, TC_KEY_F4)
            && state != APP_STATE_MENU && state != APP_STATE_PAUSED
            && state != APP_STATE_DEAD) {
            game_mode = (tc_game_mode)((game_mode + 1)
                                       % TC_GAME_MODE_COUNT);
            mining_active = 0;
            tc_log_info("game mode: %s", tc_game_mode_name(game_mode));
        }

        if (tc_window_key_pressed(&window, TC_KEY_F3)
            && state != APP_STATE_MENU && state != APP_STATE_PAUSED) {
            show_debug = !show_debug;
        }

        if (tc_window_key_pressed(&window, TC_KEY_F5)
            && state != APP_STATE_MENU && state != APP_STATE_DEAD) {
            camera_mode = (app_camera)((camera_mode + 1) % CAMERA_COUNT);
            tc_log_info("camera: %s", camera_mode_name(camera_mode));
        }

        if (tc_window_key_pressed(&window, TC_KEY_F6)
            && state != APP_STATE_MENU && state != APP_STATE_DEAD) {
            if (save_game(&options, &world, &falling, &player, &inventory,
                          game_mode)) {
                main_menu_has_continue = 1;
                app_status_set(&status, now, 0.55f, 0.95f, 0.55f, "SAVED");
            } else {
                app_status_set(&status, now, 0.95f, 0.45f, 0.40f,
                               "SAVE FAILED");
            }
        }

        if (tc_window_key_pressed(&window, TC_KEY_F9)
            && state != APP_STATE_MENU && state != APP_STATE_DEAD) {
            if (load_game(&options, &world, &player, &selected_hotbar,
                          &inventory, &game_mode, &mesh, &chunk_batch,
                          &falling, &draw_calls)) {
                state = APP_STATE_PLAYING;
                main_menu_has_continue = 1;
                menu_selection = 0;
                app_status_set(&status, now, 0.55f, 0.95f, 0.55f, "LOADED");
            } else {
                app_status_set(&status, now, 0.95f, 0.45f, 0.40f,
                               "LOAD FAILED");
            }
        }

        if (state == APP_STATE_MENU || state == APP_STATE_PAUSED) {
            app_menu_get(state, main_menu_has_continue, &menu_items,
                         &menu_count);
            if (menu_selection >= menu_count) {
                menu_selection = menu_count - 1;
            }
            if (menu_selection < 0) {
                menu_selection = 0;
            }

            if (tc_window_key_pressed(&window, TC_KEY_UP)
                || tc_window_key_pressed(&window, TC_KEY_W)) {
                menu_selection = (menu_selection + menu_count - 1)
                               % menu_count;
            }
            if (tc_window_key_pressed(&window, TC_KEY_DOWN)
                || tc_window_key_pressed(&window, TC_KEY_S)) {
                menu_selection = (menu_selection + 1) % menu_count;
            }

            if (tc_window_key_pressed(&window, TC_KEY_ENTER)
                || tc_window_key_pressed(&window, TC_KEY_SPACE)) {
                menu_action action = menu_items[menu_selection].action;

                switch (action) {
                    case MENU_NEW_WORLD:
                        if (start_new_world(&world, &player, options.seed,
                                            &selected_hotbar, &mesh,
                                            &chunk_batch, &falling,
                                            &draw_calls)) {
                            state = APP_STATE_PLAYING;
                            selected_hotbar = 0;
                            tc_inventory_init_starter(&inventory);
                            game_mode = TC_GAME_MODE_SURVIVAL;
                            tc_log_info("new world seed: %u", world.seed);
                            app_status_set(&status, now, 0.55f, 0.95f, 0.55f,
                                           "NEW WORLD");
                        } else {
                            app_status_set(&status, now, 0.95f, 0.45f, 0.40f,
                                           "WORLD FAILED");
                        }
                        break;
                    case MENU_CONTINUE:
                    case MENU_LOAD:
                        if (load_game(&options, &world, &player,
                                      &selected_hotbar, &inventory,
                                      &game_mode, &mesh, &chunk_batch,
                                      &falling, &draw_calls)) {
                            state = APP_STATE_PLAYING;
                            main_menu_has_continue = 1;
                            menu_selection = 0;
                            app_status_set(&status, now, 0.55f, 0.95f, 0.55f,
                                           "LOADED");
                        } else {
                            app_status_set(&status, now, 0.95f, 0.45f, 0.40f,
                                           "LOAD FAILED");
                        }
                        break;
                    case MENU_SAVE:
                        if (save_game(&options, &world, &falling, &player,
                                      &inventory, game_mode)) {
                            main_menu_has_continue = 1;
                            app_status_set(&status, now, 0.55f, 0.95f, 0.55f,
                                           "SAVED");
                        } else {
                            app_status_set(&status, now, 0.95f, 0.45f, 0.40f,
                                           "SAVE FAILED");
                        }
                        break;
                    case MENU_RESUME:
                        state = APP_STATE_PLAYING;
                        break;
                    case MENU_QUIT_TO_MENU:
                        state = APP_STATE_MENU;
                        tc_log_info("returned to main menu");
                        menu_selection = 0;
                        main_menu_has_continue =
                            tc_path_exists(options.save_path);
                        tc_window_set_mouse_captured(&window, 0);
                        break;
                    case MENU_QUIT:
                        running = 0;
                        break;
                }
            }
        }

        captured = 0;
        captured_before = 0;

        if (state == APP_STATE_PLAYING) {
            captured = tc_window_mouse_captured(&window);
            captured_before = captured;
            if (!options.benchmark && !captured
                && tc_window_mouse_pressed(&window, TC_MOUSE_LEFT)) {
                tc_window_set_mouse_captured(&window, 1);
                captured = 1;
            }

            if (captured) {
                float mouse_dx;
                float mouse_dy;

                tc_window_consume_mouse_delta(&window, &mouse_dx, &mouse_dy);
                tc_player_look(&player, mouse_dx, mouse_dy,
                               options.settings.sensitivity);

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
                if (game_mode == TC_GAME_MODE_SURVIVAL) {
                    input.sprint = tc_window_key_down(&window, TC_KEY_CTRL);
                    input.crouch = tc_window_key_down(&window, TC_KEY_SHIFT);
                } else {
                    input.descend = tc_window_key_down(&window, TC_KEY_CTRL)
                                 || tc_window_key_down(&window,
                                                       TC_KEY_SHIFT);
                }

                for (int index = 0; index < TC_ITEM_HOTBAR_COUNT; index++) {
                    if (tc_window_key_pressed(&window,
                                              (tc_key)(TC_KEY_1 + index))) {
                        if (selected_hotbar != index) {
                            selected_hotbar = index;
                            tc_inventory_select(&inventory, index);
                            tc_log_info("selected item: %s",
                                        tc_item_name(
                                            inventory.slots[index].item));
                        }
                    }
                }
            }

            tc_player_update_mode(&player, &world, &input, (float)dt,
                                  game_mode);
            {
                float horizontal = sqrtf(player.velocity[0]
                                         * player.velocity[0]
                                         + player.velocity[2]
                                         * player.velocity[2]);
                int moving = state == APP_STATE_PLAYING && player.on_ground
                          && horizontal > 0.05f;
                float target_walk = moving ? 1.0f : 0.0f;
                float blend = (float)dt * 10.0f;

                if (blend > 1.0f) {
                    blend = 1.0f;
                }
                if (moving) {
                    walk_phase += (float)dt * (input.sprint ? 11.0f : 7.5f);
                }
                walk_amount += (target_walk - walk_amount) * blend;
            }

            {
                int falling_landed = tc_falling_update(&falling, &world,
                                                       (float)dt);

                if (falling_landed > 0) {
                    edited = 1;
                }
            }

            if (player.position[1] < -32.0f) {
                float position[3] = {spawn_x, 40.0f, spawn_z};

                tc_log_info("player fell out of the world, respawning");
                tc_player_init(&player, position, 0.0f, -0.12f);
            }

            if (game_mode == TC_GAME_MODE_SURVIVAL
                && !player.survival.alive && state == APP_STATE_PLAYING) {
                state = APP_STATE_DEAD;
                mining_active = 0;
                mining_progress = 0.0f;
                tc_window_set_mouse_captured(&window, 0);
                tc_log_info("player died");
            }

            center_chunk_x = tc_math_floor_div((int)floorf(player.position[0]),
                                               TC_CHUNK_SIZE_X);
            center_chunk_z = tc_math_floor_div((int)floorf(player.position[2]),
                                               TC_CHUNK_SIZE_Z);

            if (center_chunk_x != world.center_chunk_x
                || center_chunk_z != world.center_chunk_z) {
                double stream_start = tc_time_now();

                if (!tc_world_update(&world, center_chunk_x, center_chunk_z)) {
                    tc_log_error("chunk streaming failed");
                    running = 0;
                } else {
                    tc_log_info("streaming to chunk (%d, %d): terrain queued "
                                "in %.2f ms",
                                center_chunk_x, center_chunk_z,
                                (tc_time_now() - stream_start) * 1000.0);
                }
            }

            if (tc_chunk_batch_should_rebase(&chunk_batch,
                                             center_chunk_x * TC_CHUNK_SIZE_X,
                                             center_chunk_z * TC_CHUNK_SIZE_Z)) {
                if (!tc_chunk_batch_rebase(&chunk_batch,
                                           center_chunk_x * TC_CHUNK_SIZE_X,
                                           center_chunk_z * TC_CHUNK_SIZE_Z)) {
                    tc_log_error("chunk batch rebase failed");
                }
            }
        }

        tc_player_get_eye(&player, eye);
        tc_player_get_forward(&player, forward);

        if (state == APP_STATE_INVENTORY) {
            int left_clicked = tc_window_mouse_pressed(&window,
                                                       TC_MOUSE_LEFT);
            int right_clicked = tc_window_mouse_pressed(&window,
                                                        TC_MOUSE_RIGHT);

            tc_window_cursor_position(&window, &cursor_x, &cursor_y);
            if (window.width > 0 && window.height > 0) {
                cursor_x *= (float)framebuffer_width / (float)window.width;
                cursor_y *= (float)framebuffer_height / (float)window.height;
            }
            inventory_hovered = inventory_slot_at(framebuffer_width,
                                                  framebuffer_height,
                                                  cursor_x, cursor_y);
            if ((left_clicked || right_clicked) && inventory_hovered >= 0) {
                tc_inventory_click(&inventory, inventory_hovered,
                                   right_clicked,
                                   tc_window_key_down(&window, TC_KEY_SHIFT));
                selected_hotbar = inventory.selected;
            }
        } else {
            inventory_hovered = -1;
        }

        if (state == APP_STATE_PLAYING && !options.benchmark && captured
            && captured_before) {
            tc_raycast_hit hit;
            uint16_t selected_item = inventory.slots[selected_hotbar].item;
            int have_hit = tc_world_raycast(&world, eye, forward, 6.0f, &hit);
            int place_clicked = tc_window_mouse_pressed(&window,
                                                        TC_MOUSE_RIGHT);
            int break_held = tc_window_mouse_down(&window, TC_MOUSE_LEFT);
            int break_clicked = tc_window_mouse_pressed(&window,
                                                        TC_MOUSE_LEFT);
            swing_continue = place_clicked
                          || (break_held && have_hit
                              && hit.block != TC_BLOCK_BEDROCK);

            if (place_clicked && have_hit
                && game_mode != TC_GAME_MODE_SPECTATOR) {
                uint8_t block = tc_item_block(selected_item);

                if (block != TC_BLOCK_AIR
                    && inventory.slots[selected_hotbar].count > 0
                    && !tc_player_intersects_block(&player,
                                                   hit.previous_x,
                                                   hit.previous_y,
                                                   hit.previous_z)
                    && tc_world_get_block(&world, hit.previous_x,
                                          hit.previous_y,
                                          hit.previous_z) == TC_BLOCK_AIR) {
                    double remesh_ms = 0.0;

                    if (apply_block_edit(&world, &mesh, &chunk_batch,
                                         &falling, &draw_calls,
                                         hit.previous_x, hit.previous_y,
                                         hit.previous_z, block, &remesh_ms)) {
                        edited = 1;
                        if (game_mode == TC_GAME_MODE_SURVIVAL) {
                            tc_inventory_consume_selected(&inventory, 1);
                        }
                        tc_log_info("placed %s at (%d, %d, %d), "
                                    "remesh %.2f ms",
                                    tc_item_name(selected_item),
                                    hit.previous_x, hit.previous_y,
                                    hit.previous_z, remesh_ms);
                    }
                }
            }

            if (game_mode != TC_GAME_MODE_SPECTATOR && break_held && have_hit
                && hit.block != TC_BLOCK_BEDROCK) {
                float seconds = tc_block_break_seconds(hit.block,
                                                       selected_item);

                if (game_mode == TC_GAME_MODE_CREATIVE) {
                    if (break_clicked) {
                        double remesh_ms = 0.0;

                        if (apply_block_edit(&world, &mesh, &chunk_batch,
                                             &falling, &draw_calls,
                                             hit.x, hit.y, hit.z,
                                             TC_BLOCK_AIR, &remesh_ms)) {
                            edited = 1;
                            tc_log_info("broke block at (%d, %d, %d), "
                                        "remesh %.2f ms",
                                        hit.x, hit.y, hit.z, remesh_ms);
                        }
                    }
                } else if (seconds > 0.0f) {
                    if (!mining_active || mining_x != hit.x
                        || mining_y != hit.y || mining_z != hit.z) {
                        mining_active = 1;
                        mining_x = hit.x;
                        mining_y = hit.y;
                        mining_z = hit.z;
                        mining_progress = 0.0f;
                    }
                    mining_progress += (float)dt / seconds;
                    if (mining_progress >= 1.0f) {
                        double remesh_ms = 0.0;

                        if (apply_block_edit(&world, &mesh, &chunk_batch,
                                             &falling, &draw_calls,
                                             hit.x, hit.y, hit.z,
                                             TC_BLOCK_AIR, &remesh_ms)) {
                            uint16_t drop = tc_item_mining_drop(
                                hit.block, selected_item);

                            edited = 1;
                            if (drop != TC_ITEM_NONE) {
                                uint16_t leftover = tc_inventory_add(
                                    &inventory, drop, 1);

                                if (leftover > 0) {
                                    tc_log_info("inventory full, lost drop");
                                }
                            }
                            tc_inventory_damage_selected(&inventory);
                            if (game_mode == TC_GAME_MODE_SURVIVAL) {
                                tc_survival_add_exhaustion(
                                    &player.survival, 0.005f);
                            }
                            tc_log_info("broke block at (%d, %d, %d), "
                                        "remesh %.2f ms",
                                        hit.x, hit.y, hit.z, remesh_ms);
                        }
                        mining_active = 0;
                        mining_progress = 0.0f;
                    }
                }
            } else {
                mining_active = 0;
                mining_progress = 0.0f;
            }
        } else if (state == APP_STATE_PLAYING) {
            mining_active = 0;
            mining_progress = 0.0f;
        }

        if (swing_continue && !hand_swinging) {
            hand_swinging = 1;
            hand_swing = 0.0f;
        }
        if (hand_swinging) {
            hand_swing += (float)dt / 0.28f;
            if (hand_swing >= 1.0f) {
                if (swing_continue) {
                    hand_swing -= 1.0f;
                } else {
                    hand_swinging = 0;
                    hand_swing = 0.0f;
                }
            }
        }

        if (state == APP_STATE_PLAYING) {
            tc_mesh_stats remesh_stats;
            size_t remesh_count = 0;
            size_t remesh_pending = 0;
            size_t remesh_limit = edited ? 1 : 6;

            if (!rebuild_dirty_chunks(&world, &mesh, &chunk_batch,
                                      remesh_limit, 3.0, -1,
                                      &remesh_stats, &remesh_count,
                                      &remesh_pending, &draw_calls)) {
                tc_log_error("chunk remeshing failed");
                running = 0;
            }
        }

        compute_camera(&world, &player, camera_mode, eye, target);

        {
            const float pi = 3.14159265358979323846f;
            float projection[16];
            float view[16];
            float mvp[16];
            float up[3] = {0.0f, 1.0f, 0.0f};
            float aspect = (float)framebuffer_width
                         / (float)framebuffer_height;

            tc_mat4_perspective(projection,
                                TC_RENDER_FOV_DEGREES * pi / 180.0f, aspect,
                                TC_RENDER_NEAR_PLANE,
                                TC_RENDER_FAR_PLANE);
            tc_mat4_look_at(view, eye, target, up);
            tc_mat4_mul(mvp, projection, view);
            memcpy(camera_mvp, mvp, sizeof(camera_mvp));
            extract_frustum_planes(mvp, frustum_planes);
        }

        for (size_t index = 0; index < slot_count; index++) {
            float distance;

            if (tc_chunk_batch_index_count(&chunk_batch, (int)index) == 0
                || !chunk_in_frustum(&world, index, &frustum_planes[0][0])) {
                continue;
            }

            distance = chunk_distance_sq(&world, index, eye);
            if (distance > fog_end * fog_end) {
                continue;
            }

            draw_distances[index] = distance;
            draw_order[visible_chunks++] = index;
        }

        for (size_t index = 1; index < visible_chunks; index++) {
            size_t item = draw_order[index];
            float item_distance = draw_distances[item];
            size_t position = index;

            while (position > 0
                   && item_distance
                      < draw_distances[draw_order[position - 1]]) {
                draw_order[position] = draw_order[position - 1];
                position--;
            }
            draw_order[position] = item;
        }

        tc_renderer_begin_frame(&renderer, eye, target, framebuffer_width,
                                framebuffer_height);

        held_item = inventory.slots[selected_hotbar].item;
        if (inventory.slots[selected_hotbar].count == 0) {
            held_item = TC_ITEM_NONE;
        }

        if (visible_chunks > 0) {
            float batch_origin[3];

            tc_chunk_batch_origin(&chunk_batch, batch_origin);
            tc_renderer_set_chunk_origin(&renderer, batch_origin[0],
                                         batch_origin[1], batch_origin[2]);
            tc_chunk_batch_draw(&chunk_batch, (const int *)draw_order,
                                visible_chunks);
        }

        if (tc_falling_count(&falling) > 0) {
            const tc_falling_block *blocks = tc_falling_data(&falling);
            size_t count = tc_falling_count(&falling);

            tc_renderer_begin_models(&renderer, camera_mvp);
            for (size_t index = 0; index < count; index++) {
                float position[3] = {
                    (float)blocks[index].x,
                    blocks[index].y,
                    (float)blocks[index].z
                };

                tc_entity_draw_block(&entities, &renderer, blocks[index].block,
                                     position);
            }
            tc_renderer_end_models(&renderer);
        }

        if (camera_mode != CAMERA_FIRST_PERSON
            && game_mode != TC_GAME_MODE_SPECTATOR) {
            tc_renderer_begin_models(&renderer, camera_mvp);
            tc_entity_draw_player(&entities, &renderer, &player, held_item,
                                  hand_swing, walk_amount, walk_phase);
            tc_renderer_end_models(&renderer);
        }

        tc_renderer_end_frame(&renderer);

        if (camera_mode == CAMERA_FIRST_PERSON
            && state == APP_STATE_PLAYING
            && game_mode != TC_GAME_MODE_SPECTATOR) {
            glDisable(GL_DEPTH_TEST);
            tc_renderer_begin_models(&renderer, camera_mvp);
            tc_entity_draw_first_person(&entities, &renderer, eye,
                                        player.yaw, player.pitch, held_item,
                                        hand_swing, walk_amount, walk_phase);
            tc_renderer_end_models(&renderer);
            glEnable(GL_DEPTH_TEST);
        }

        if (state == APP_STATE_PLAYING || state == APP_STATE_INVENTORY) {
            hotbar_fill_swatches(&inventory, hud_swatches);
            tc_hud_draw_hotbar(framebuffer_width, framebuffer_height,
                               hud_swatches, TC_ITEM_HOTBAR_COUNT,
                               selected_hotbar);
            if (game_mode == TC_GAME_MODE_SURVIVAL) {
                tc_hud_draw_survival(framebuffer_width, framebuffer_height,
                                     &player.survival);
            }
            if (captured) {
                tc_hud_draw_crosshair(framebuffer_width, framebuffer_height);
            }
            if (mining_active && mining_progress > 0.0f) {
                int bar_width = 96;
                int bar_x = (framebuffer_width - bar_width) / 2;
                int bar_y = framebuffer_height / 2 + 18;

                ui_rect_top(framebuffer_height, bar_x - 2, bar_y - 2,
                            bar_width + 4, 8, 0.06f, 0.06f, 0.07f);
                ui_rect_top(framebuffer_height, bar_x, bar_y, bar_width, 4,
                            0.62f, 0.86f, 0.44f);
            }
        }

        if (state == APP_STATE_INVENTORY) {
            inventory_draw(&text, framebuffer_width, framebuffer_height,
                           &inventory, inventory_hovered, &player, game_mode,
                           cursor_x, cursor_y);
        } else if (state == APP_STATE_DEAD) {
            char death_line[64];

            tc_hud_draw_rect(0, 0, framebuffer_width, framebuffer_height,
                             0.16f, 0.02f, 0.02f);
            tc_text_begin(&text, framebuffer_width, framebuffer_height);
            draw_text_centered(&text, "YOU DIED", framebuffer_width,
                               framebuffer_height / 2 - 34, 3.0f,
                               0.92f, 0.20f, 0.18f);
            snprintf(death_line, sizeof(death_line),
                     "PRESS SPACE TO RESPAWN");
            draw_text_centered(&text, death_line, framebuffer_width,
                               framebuffer_height / 2 + 24, 1.5f,
                               0.95f, 0.92f, 0.86f);
            tc_text_end(&text);
        } else if (state == APP_STATE_MENU || state == APP_STATE_PAUSED) {
            app_menu_get(state, main_menu_has_continue, &menu_items,
                         &menu_count);
            if (menu_selection >= menu_count) {
                menu_selection = menu_count - 1;
            }
            if (menu_selection < 0) {
                menu_selection = 0;
            }

            draw_menu(&text, framebuffer_width, framebuffer_height, menu_items,
                      menu_count, menu_selection,
                      state == APP_STATE_PAUSED ? "PAUSED" : "THINKCRAFT",
                      &status, now, state == APP_STATE_PAUSED);
        }

        if (state == APP_STATE_PLAYING) {
            const tc_item_stack *selected_stack =
                &inventory.slots[selected_hotbar];
            char line[64];

            tc_text_begin(&text, framebuffer_width, framebuffer_height);
            if (selected_stack->item != TC_ITEM_NONE
                && selected_stack->count > 0) {
                snprintf(line, sizeof(line), "%s X%u",
                         tc_item_name(selected_stack->item),
                         (unsigned)selected_stack->count);
            } else {
                snprintf(line, sizeof(line), "EMPTY");
            }
            tc_text_draw(&text, line,
                         (framebuffer_width - tc_text_width(line, 1.0f)) / 2,
                         framebuffer_height - 58, 1.0f,
                         0.95f, 0.95f, 0.90f);
            tc_text_draw(&text, tc_game_mode_name(game_mode), 8, 8, 1.0f,
                         0.78f, 0.82f, 0.86f);
            tc_text_end(&text);
        }

        if (show_debug) {
            app_debug_info info;

            info.fps = display_fps;
            info.frame_ms = dt * 1000.0;
            info.position[0] = player.position[0];
            info.position[1] = player.position[1];
            info.position[2] = player.position[2];
            info.block_x = (int)floorf(player.position[0]);
            info.block_y = (int)floorf(player.position[1]);
            info.block_z = (int)floorf(player.position[2]);
            info.chunk_x = world.center_chunk_x;
            info.chunk_z = world.center_chunk_z;
            info.seed = world.seed;
            info.facing = facing_name(forward);
            info.loaded_chunks = tc_world_loaded_count(&world);
            info.visible_chunks = visible_chunks;
            info.slot_count = tc_world_capacity(&world);
            info.render_distance = world.radius;
            info.render_scale = render_scale;
            info.health = player.survival.health;
            info.hunger = player.survival.hunger;
            info.rss_bytes = rss_bytes;
            draw_debug_overlay(&text, framebuffer_width, framebuffer_height,
                               &info);
        }

        tc_window_swap(&window);

        frames_since_report++;
        total_frames++;

        now = tc_window_time();
        elapsed = now - last_fps_time;
        if (elapsed >= 1.0) {
            double fps = (double)frames_since_report / elapsed;
            const char *mode = state == APP_STATE_MENU ? "menu"
                             : state == APP_STATE_PAUSED ? "paused"
                             : tc_item_name(
                                   inventory.slots[selected_hotbar].item);

            snprintf(title, sizeof(title),
                     "ThinkCraft | chunk (%d, %d) | %s | %.1f FPS | %dx%d",
                     world.center_chunk_x, world.center_chunk_z, mode, fps,
                     framebuffer_width, framebuffer_height);
            tc_window_set_title(&window, title);
            tc_log_info("%.1f FPS, %zu visible chunks", fps, visible_chunks);
            display_fps = fps;
            rss_bytes = tc_platform_rss_bytes();
            frames_since_report = 0;
            last_fps_time = now;

            if (options.settings.render_scale <= 0.0f
                && !options.settings.vsync) {
                float desired = render_scale
                              * sqrtf((float)(fps
                                              / options.settings.target_fps));

                if (desired < 0.5f) {
                    desired = 0.5f;
                } else if (desired > 1.0f) {
                    desired = 1.0f;
                }

                if (fabsf(desired - render_scale) > 0.01f) {
                    int render_width;
                    int render_height;

                    render_scale = desired;
                    tc_renderer_set_render_scale(&renderer, render_scale);
                    render_width = (int)((float)framebuffer_width
                                         * render_scale + 0.5f);
                    render_height = (int)((float)framebuffer_height
                                          * render_scale + 0.5f);
                    tc_log_info("render scale: %.2f (%dx%d internal, "
                                "%.1f FPS target)",
                                render_scale, render_width, render_height,
                                options.settings.target_fps);
                }
            }
        }

        if (options.benchmark && now - start_time >= options.benchmark_seconds) {
            running = 0;
        }

        if (options.settings.fps_cap > 0) {
            double target_time = frame_start
                               + 1.0 / (double)options.settings.fps_cap;
            double current_time = tc_window_time();

            if (target_time > current_time) {
                tc_sleep_ms((target_time - current_time) * 1000.0);
            }
        }

        tc_window_end_frame(&window);
    }

    total_elapsed = tc_window_time() - start_time;
    if (total_frames > 0 && total_elapsed > 0.0) {
        average_fps = (double)total_frames / total_elapsed;
    }

    if (options.benchmark) {
        tc_log_info("benchmark: %.2f s, %lld frames, %.1f FPS average, "
                    "%.3f ms/frame (render scale %.2fx)",
                    total_elapsed, total_frames, average_fps,
                    total_frames > 0
                        ? (total_elapsed * 1000.0 / (double)total_frames)
                        : 0.0, render_scale);
        tc_log_info("benchmark complete");
    } else {
        tc_log_info("session: %.2f s, %lld frames, %.1f FPS average "
                    "(render scale %.2fx)",
                    total_elapsed, total_frames, average_fps, render_scale);
    }

    if (options.config_path[0] != '\0'
        && !tc_settings_save(&options.settings, options.config_path)) {
        tc_log_error("could not save settings to %s", options.config_path);
    }

    exit_code = 0;

cleanup:
    if (mesh_initialized) {
        tc_mesh_shutdown(&mesh);
    }
    if (chunk_batch_initialized) {
        tc_chunk_batch_shutdown(&chunk_batch);
    }
    if (entities_initialized) {
        tc_entity_renderer_shutdown(&entities, &renderer);
    }
    if (hud_initialized) {
        tc_hud_shutdown();
    }
    if (text_initialized) {
        tc_text_shutdown(&text);
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
