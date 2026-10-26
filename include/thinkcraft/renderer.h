#ifndef THINKCRAFT_RENDERER_H
#define THINKCRAFT_RENDERER_H

#include "thinkcraft/arena.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/shader.h"

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    unsigned int texture;
    unsigned int index_count;
    tc_shader shader;
    int u_mvp;
    int u_texture;
} tc_renderer;

int tc_renderer_init(tc_renderer *renderer, tc_arena *arena, const char *shader_dir);
int tc_renderer_upload_mesh(tc_renderer *renderer, const tc_mesh *mesh);
void tc_renderer_shutdown(tc_renderer *renderer);
void tc_renderer_frame(tc_renderer *renderer, double time_seconds,
                       int framebuffer_width, int framebuffer_height);

#endif
