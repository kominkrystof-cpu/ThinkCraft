#include "thinkcraft/hud.h"
#include "thinkcraft/log.h"
#include "thinkcraft/shader.h"

#include <glad/gl.h>

#include <stddef.h>
#include <stdio.h>

#define HUD_MAX_RECTS 4096
#define HUD_FLOATS_PER_VERTEX 6
#define HUD_VERTICES_PER_RECT 6
#define HUD_VERTEX_FLOATS \
    (HUD_MAX_RECTS * HUD_VERTICES_PER_RECT * HUD_FLOATS_PER_VERTEX)

static tc_shader hud_shader;
static unsigned int hud_vao;
static unsigned int hud_vbo;
static int hud_u_viewport;
static int hud_initialized;
static int hud_fb_width;
static int hud_fb_height;
static float hud_vertices[HUD_VERTEX_FLOATS];
static size_t hud_float_count;

int tc_hud_init(tc_arena *arena, const char *shader_dir) {
    char vertex_path[4096];
    char fragment_path[4096];

    if (hud_initialized) {
        return 1;
    }
    if (arena == NULL || shader_dir == NULL) {
        return 0;
    }

    snprintf(vertex_path, sizeof(vertex_path), "%s/hud.vert", shader_dir);
    snprintf(fragment_path, sizeof(fragment_path), "%s/hud.frag", shader_dir);

    if (!tc_shader_load(&hud_shader, arena, vertex_path, fragment_path)) {
        return 0;
    }

    hud_u_viewport = tc_shader_uniform(&hud_shader, "u_viewport");
    if (hud_u_viewport < 0) {
        tc_log_error("hud shader is missing u_viewport");
        tc_shader_destroy(&hud_shader);
        return 0;
    }

    glGenVertexArrays(1, &hud_vao);
    glGenBuffers(1, &hud_vbo);
    if (hud_vao == 0 || hud_vbo == 0) {
        tc_hud_shutdown();
        return 0;
    }

    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(hud_vertices), NULL,
                 GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          HUD_FLOATS_PER_VERTEX * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          HUD_FLOATS_PER_VERTEX * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    hud_initialized = 1;
    return 1;
}

void tc_hud_shutdown(void) {
    if (hud_vbo != 0) {
        glDeleteBuffers(1, &hud_vbo);
        hud_vbo = 0;
    }
    if (hud_vao != 0) {
        glDeleteVertexArrays(1, &hud_vao);
        hud_vao = 0;
    }
    tc_shader_destroy(&hud_shader);
    hud_initialized = 0;
    hud_u_viewport = -1;
    hud_float_count = 0;
}

static void hud_flush(void) {
    if (!hud_initialized || hud_float_count == 0) {
        return;
    }

    tc_shader_bind(&hud_shader);
    glUniform2f(hud_u_viewport, (float)hud_fb_width,
                (float)hud_fb_height);
    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(hud_float_count * sizeof(float)),
                    hud_vertices);
    glDrawArrays(GL_TRIANGLES, 0,
                 (GLsizei)(hud_float_count / HUD_FLOATS_PER_VERTEX));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    hud_float_count = 0;
}

static void hud_begin(int framebuffer_width, int framebuffer_height) {
    hud_fb_width = framebuffer_width;
    hud_fb_height = framebuffer_height;
    hud_float_count = 0;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, framebuffer_width, framebuffer_height);
    glViewport(0, 0, framebuffer_width, framebuffer_height);
}

static void hud_end(void) {
    hud_flush();
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

static void fill_rect(int x, int y, int width, int height,
                      float red, float green, float blue) {
    float x0;
    float y0;
    float x1;
    float y1;
    size_t index;

    if (width <= 0 || height <= 0 || !hud_initialized) {
        return;
    }

    if (hud_float_count
        + (size_t)HUD_VERTICES_PER_RECT * HUD_FLOATS_PER_VERTEX
        > (size_t)HUD_VERTEX_FLOATS) {
        hud_flush();
    }

    x0 = (float)x;
    y0 = (float)y;
    x1 = (float)(x + width);
    y1 = (float)(y + height);
    index = hud_float_count;

    hud_vertices[index + 0] = x0;
    hud_vertices[index + 1] = y0;
    hud_vertices[index + 2] = red;
    hud_vertices[index + 3] = green;
    hud_vertices[index + 4] = blue;
    hud_vertices[index + 5] = 1.0f;

    hud_vertices[index + 6] = x1;
    hud_vertices[index + 7] = y0;
    hud_vertices[index + 8] = red;
    hud_vertices[index + 9] = green;
    hud_vertices[index + 10] = blue;
    hud_vertices[index + 11] = 1.0f;

    hud_vertices[index + 12] = x1;
    hud_vertices[index + 13] = y1;
    hud_vertices[index + 14] = red;
    hud_vertices[index + 15] = green;
    hud_vertices[index + 16] = blue;
    hud_vertices[index + 17] = 1.0f;

    hud_vertices[index + 18] = x0;
    hud_vertices[index + 19] = y0;
    hud_vertices[index + 20] = red;
    hud_vertices[index + 21] = green;
    hud_vertices[index + 22] = blue;
    hud_vertices[index + 23] = 1.0f;

    hud_vertices[index + 24] = x1;
    hud_vertices[index + 25] = y1;
    hud_vertices[index + 26] = red;
    hud_vertices[index + 27] = green;
    hud_vertices[index + 28] = blue;
    hud_vertices[index + 29] = 1.0f;

    hud_vertices[index + 30] = x0;
    hud_vertices[index + 31] = y1;
    hud_vertices[index + 32] = red;
    hud_vertices[index + 33] = green;
    hud_vertices[index + 34] = blue;
    hud_vertices[index + 35] = 1.0f;

    hud_float_count += HUD_VERTICES_PER_RECT * HUD_FLOATS_PER_VERTEX;
}

void tc_hud_draw_hotbar(int framebuffer_width, int framebuffer_height,
                        const tc_hud_swatch *swatches, int count,
                        int selected) {
    const int slot_size = 28;
    const int gap = 4;
    const int padding = 7;
    int panel_width;
    int panel_height;
    int panel_x;
    int panel_y;

    if (framebuffer_width <= 0 || framebuffer_height <= 0
        || swatches == NULL || count <= 0) {
        return;
    }

    panel_width = count * slot_size + (count - 1) * gap + padding * 2;
    panel_height = slot_size + padding * 2;
    panel_x = (framebuffer_width - panel_width) / 2;
    panel_y = 16;

    hud_begin(framebuffer_width, framebuffer_height);

    fill_rect(panel_x, panel_y, panel_width, panel_height,
              0.07f, 0.08f, 0.09f);

    for (int index = 0; index < count; index++) {
        const tc_hud_swatch *swatch = &swatches[index];
        int x = panel_x + padding + index * (slot_size + gap);
        int y = panel_y + padding;

        if (index == selected) {
            fill_rect(x - 2, y - 2, slot_size + 4, slot_size + 4,
                      0.96f, 0.96f, 0.92f);
        } else {
            fill_rect(x - 1, y - 1, slot_size + 2, slot_size + 2,
                      0.18f, 0.19f, 0.20f);
        }

        fill_rect(x, y, slot_size, slot_size,
                  swatch->color[0], swatch->color[1], swatch->color[2]);
    }

    hud_end();
}

void tc_hud_draw_rect(int x, int y, int width, int height,
                      float red, float green, float blue) {
    GLint viewport[4];

    if (width <= 0 || height <= 0) {
        return;
    }

    glGetIntegerv(GL_VIEWPORT, viewport);
    hud_begin(viewport[2], viewport[3]);
    fill_rect(x, y, width, height, red, green, blue);
    hud_end();
}

static void draw_heart(int x, int y, float value) {
    int half = value < 2.0f;

    fill_rect(x, y, 10, 9, 0.12f, 0.03f, 0.03f);
    if (value < 1.0f) {
        return;
    }

    fill_rect(x + 1, y + 6, 3, 2, 0.86f, 0.13f, 0.16f);
    fill_rect(x + 2, y + 3, half ? 3 : 6, 3, 0.86f, 0.13f, 0.16f);
    fill_rect(x + 3, y + 1, half ? 2 : 4, 2, 0.86f, 0.13f, 0.16f);
    if (!half) {
        fill_rect(x + 6, y + 6, 3, 2, 0.86f, 0.13f, 0.16f);
    }
}

static void draw_hunger(int x, int y, float value) {
    int half = value < 2.0f;

    fill_rect(x, y, 10, 9, 0.10f, 0.06f, 0.03f);
    if (value < 1.0f) {
        return;
    }

    fill_rect(x + 1, y + 2, half ? 3 : 7, 5, 0.72f, 0.45f, 0.20f);
    if (!half) {
        fill_rect(x + 7, y + 6, 3, 2, 0.86f, 0.80f, 0.62f);
    }
}

void tc_hud_draw_survival(int framebuffer_width, int framebuffer_height,
                          const tc_survival *survival) {
    const int slot_size = 28;
    const int gap = 4;
    const int padding = 7;
    const int icon_step = 12;
    const int icon_row_width = 10 * icon_step - 2;
    const int panel_width = 9 * slot_size + 8 * gap + padding * 2;
    const int panel_x = (framebuffer_width - panel_width) / 2;
    const int icon_y = 16 + slot_size + padding * 2 + 7;
    const int heart_x = panel_x;
    const int hunger_x = panel_x + panel_width - icon_row_width;

    if (framebuffer_width <= 0 || framebuffer_height <= 0
        || survival == NULL) {
        return;
    }

    hud_begin(framebuffer_width, framebuffer_height);

    for (int index = 0; index < 10; index++) {
        draw_heart(heart_x + index * icon_step, icon_y,
                   survival->health - (float)index * 2.0f);
        draw_hunger(hunger_x + index * icon_step, icon_y,
                    survival->hunger - (float)index * 2.0f);
    }

    if (survival->damage_flash > 0.0f) {
        fill_rect(0, 0, framebuffer_width, 6, 0.55f, 0.08f, 0.08f);
        fill_rect(0, framebuffer_height - 6, framebuffer_width, 6,
                  0.55f, 0.08f, 0.08f);
        fill_rect(0, 0, 6, framebuffer_height, 0.55f, 0.08f, 0.08f);
        fill_rect(framebuffer_width - 6, 0, 6, framebuffer_height,
                  0.55f, 0.08f, 0.08f);
    }

    hud_end();
}

void tc_hud_draw_crosshair(int framebuffer_width, int framebuffer_height) {
    int center_x;
    int center_y;

    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    center_x = framebuffer_width / 2;
    center_y = framebuffer_height / 2;

    hud_begin(framebuffer_width, framebuffer_height);

    fill_rect(center_x - 6, center_y - 1, 13, 3, 0.05f, 0.05f, 0.05f);
    fill_rect(center_x - 1, center_y - 6, 3, 13, 0.05f, 0.05f, 0.05f);
    fill_rect(center_x - 5, center_y, 11, 1, 0.96f, 0.96f, 0.92f);
    fill_rect(center_x, center_y - 5, 1, 11, 0.96f, 0.96f, 0.92f);

    hud_end();
}
