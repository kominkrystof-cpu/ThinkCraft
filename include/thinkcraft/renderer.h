#ifndef THINKCRAFT_RENDERER_H
#define THINKCRAFT_RENDERER_H

#include "thinkcraft/arena.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/shader.h"

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    unsigned int index_count;
} tc_render_mesh;

typedef struct {
    unsigned int texture;
    tc_shader shader;
    int u_mvp;
    int u_texture;
    int u_camera_pos;
    int u_fog_color;
    int u_fog_start;
    int u_fog_end;
} tc_renderer;

int tc_renderer_init(tc_renderer *renderer, tc_arena *arena, const char *shader_dir);
int tc_renderer_upload_mesh(tc_renderer *renderer, tc_render_mesh *target,
                            const tc_mesh *mesh);
void tc_renderer_destroy_mesh(tc_render_mesh *mesh);
void tc_renderer_begin_frame(tc_renderer *renderer, const float eye[3],
                             const float target[3], int framebuffer_width,
                             int framebuffer_height);
void tc_renderer_draw_mesh(tc_renderer *renderer, const tc_render_mesh *mesh);
void tc_renderer_shutdown(tc_renderer *renderer);

#endif
