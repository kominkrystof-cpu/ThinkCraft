#ifndef THINKCRAFT_RENDERER_H
#define THINKCRAFT_RENDERER_H

#include "thinkcraft/arena.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/shader.h"

#define TC_RENDER_FOV_DEGREES 60.0f
#define TC_RENDER_NEAR_PLANE 0.1f
#define TC_RENDER_FAR_PLANE 256.0f
#define TC_RENDER_FOG_START 64.0f
#define TC_RENDER_FOG_END 80.0f

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    unsigned int index_count;
    size_t vbo_capacity;
    size_t ebo_capacity;
    float origin[3];
} tc_render_mesh;

typedef struct {
    unsigned int texture;
    tc_shader shader;
    tc_shader model_shader;
    int u_mvp;
    int u_texture;
    int u_camera_pos;
    int u_chunk_origin;
    int u_fog_color;
    int u_fog_start;
    int u_fog_end;
    int u_model_view_projection;
    int u_model;
    int u_model_texture;
    unsigned int scene_fbo;
    unsigned int scene_color;
    unsigned int scene_depth;
    int scene_width;
    int scene_height;
    int window_width;
    int window_height;
    int use_scene_fbo;
    float render_scale;
    float fog_start;
    float fog_end;
} tc_renderer;

int tc_renderer_init(tc_renderer *renderer, tc_arena *arena, const char *shader_dir);
int tc_renderer_upload_mesh(tc_renderer *renderer, tc_render_mesh *target,
                            const tc_mesh *mesh);
void tc_renderer_destroy_mesh(tc_render_mesh *mesh);
void tc_renderer_set_render_scale(tc_renderer *renderer, float scale);
void tc_renderer_set_fog(tc_renderer *renderer, float start, float end);

static inline void tc_renderer_fog_for_radius(int radius, float *start,
                                              float *end) {
    float distance = (float)(radius * 16 + 16);

    if (start != NULL) {
        *start = distance * 0.8f;
    }
    if (end != NULL) {
        *end = distance;
    }
}
void tc_renderer_begin_frame(tc_renderer *renderer, const float eye[3],
                             const float target[3], int framebuffer_width,
                             int framebuffer_height);
void tc_renderer_draw_mesh(tc_renderer *renderer, const tc_render_mesh *mesh);
void tc_renderer_begin_models(tc_renderer *renderer,
                              const float view_projection[16]);
void tc_renderer_draw_model(tc_renderer *renderer,
                            const tc_render_mesh *mesh,
                            const float model[16]);
void tc_renderer_end_models(tc_renderer *renderer);
void tc_renderer_end_frame(tc_renderer *renderer);
void tc_renderer_shutdown(tc_renderer *renderer);

#endif
