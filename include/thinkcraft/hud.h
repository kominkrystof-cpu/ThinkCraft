#ifndef THINKCRAFT_HUD_H
#define THINKCRAFT_HUD_H

#include "thinkcraft/arena.h"
#include "thinkcraft/survival.h"

typedef struct {
    float color[3];
} tc_hud_swatch;

int tc_hud_init(tc_arena *arena, const char *shader_dir);
void tc_hud_shutdown(void);

void tc_hud_draw_hotbar(int framebuffer_width, int framebuffer_height,
                        const tc_hud_swatch *swatches, int count,
                        int selected);
void tc_hud_draw_crosshair(int framebuffer_width, int framebuffer_height);
void tc_hud_draw_rect(int x, int y, int width, int height,
                      float red, float green, float blue);
void tc_hud_draw_survival(int framebuffer_width, int framebuffer_height,
                          const tc_survival *survival);

#endif
