#ifndef THINKCRAFT_HUD_H
#define THINKCRAFT_HUD_H

typedef struct {
    float color[3];
} tc_hud_swatch;

void tc_hud_draw_hotbar(int framebuffer_width, int framebuffer_height,
                        const tc_hud_swatch *swatches, int count,
                        int selected);
void tc_hud_draw_crosshair(int framebuffer_width, int framebuffer_height);

#endif
