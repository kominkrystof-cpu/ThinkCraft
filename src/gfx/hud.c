#include "thinkcraft/hud.h"

#include <glad/gl.h>

#include <stddef.h>

static void fill_rect(int x, int y, int width, int height,
                      float red, float green, float blue) {
    if (width <= 0 || height <= 0) {
        return;
    }

    glScissor(x, y, width, height);
    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
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

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

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

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
}

void tc_hud_draw_rect(int x, int y, int width, int height,
                      float red, float green, float blue) {
    if (width <= 0 || height <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    fill_rect(x, y, width, height, red, green, blue);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
}

void tc_hud_draw_crosshair(int framebuffer_width, int framebuffer_height) {
    int center_x;
    int center_y;

    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    center_x = framebuffer_width / 2;
    center_y = framebuffer_height / 2;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    fill_rect(center_x - 6, center_y - 1, 13, 3, 0.05f, 0.05f, 0.05f);
    fill_rect(center_x - 1, center_y - 6, 3, 13, 0.05f, 0.05f, 0.05f);
    fill_rect(center_x - 5, center_y, 11, 1, 0.96f, 0.96f, 0.92f);
    fill_rect(center_x, center_y - 5, 1, 11, 0.96f, 0.96f, 0.92f);

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
}
