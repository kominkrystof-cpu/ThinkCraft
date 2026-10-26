#ifndef THINKCRAFT_TEXT_H
#define THINKCRAFT_TEXT_H

#include "thinkcraft/arena.h"
#include "thinkcraft/shader.h"

typedef struct {
    tc_shader shader;
    unsigned int texture;
    unsigned int vao;
    unsigned int vbo;
    int u_viewport;
    int u_color;
    int u_font;
    int initialized;
} tc_text_renderer;

int tc_text_init(tc_text_renderer *text, tc_arena *arena,
                 const char *shader_dir);
void tc_text_shutdown(tc_text_renderer *text);
void tc_text_begin(tc_text_renderer *text, int framebuffer_width,
                   int framebuffer_height);
void tc_text_draw(tc_text_renderer *text, const char *string, int x, int y,
                  float scale, float red, float green, float blue);
void tc_text_end(tc_text_renderer *text);
int tc_text_width(const char *string, float scale);

#endif
