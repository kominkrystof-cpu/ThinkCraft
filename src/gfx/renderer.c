#include "thinkcraft/renderer.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"
#include "thinkcraft/texture.h"

#include <glad/gl.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void copy_atlas_tile(const uint8_t *atlas, uint8_t *tile_pixels,
                            int tile) {
    int tile_x = (tile % TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    int tile_y = (tile / TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;

    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        memcpy(tile_pixels + (size_t)y * TC_ATLAS_TILE_SIZE * 4,
               atlas + ((size_t)(tile_y + y) * TC_ATLAS_SIZE
                        + (size_t)tile_x) * 4,
               TC_ATLAS_TILE_SIZE * 4);
    }
}

static void create_atlas_texture(unsigned int *texture) {
    uint8_t atlas[TC_ATLAS_PIXEL_COUNT];
    uint8_t tile_pixels[TC_ATLAS_TILE_SIZE * TC_ATLAS_TILE_SIZE * 4];

    tc_texture_generate_atlas(atlas);

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 TC_ATLAS_TILE_SIZE, TC_ATLAS_TILE_SIZE, TC_TILE_COUNT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    for (int tile = 0; tile < TC_TILE_COUNT; tile++) {
        copy_atlas_tile(atlas, tile_pixels, tile);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, tile,
                        TC_ATLAS_TILE_SIZE, TC_ATLAS_TILE_SIZE, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, tile_pixels);
    }
}

static void configure_mesh(tc_render_mesh *mesh) {
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, tile));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBindVertexArray(0);
}

static void destroy_scene_target(tc_renderer *renderer) {
    if (renderer->scene_fbo != 0) {
        glDeleteFramebuffers(1, &renderer->scene_fbo);
    }
    if (renderer->scene_color != 0) {
        glDeleteTextures(1, &renderer->scene_color);
    }
    if (renderer->scene_depth != 0) {
        glDeleteRenderbuffers(1, &renderer->scene_depth);
    }

    renderer->scene_fbo = 0;
    renderer->scene_color = 0;
    renderer->scene_depth = 0;
    renderer->scene_width = 0;
    renderer->scene_height = 0;
}

static int ensure_scene_target(tc_renderer *renderer, int width, int height) {
    if (renderer->scene_fbo != 0 && renderer->scene_width == width
        && renderer->scene_height == height) {
        return 1;
    }

    destroy_scene_target(renderer);

    glGenFramebuffers(1, &renderer->scene_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->scene_fbo);

    glGenTextures(1, &renderer->scene_color);
    glBindTexture(GL_TEXTURE_2D, renderer->scene_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, renderer->scene_color, 0);

    glGenRenderbuffers(1, &renderer->scene_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, renderer->scene_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, renderer->scene_depth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        != GL_FRAMEBUFFER_COMPLETE) {
        tc_log_error("scene framebuffer is incomplete");
        destroy_scene_target(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return 0;
    }

    renderer->scene_width = width;
    renderer->scene_height = height;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 1;
}

int tc_renderer_init(tc_renderer *renderer, tc_arena *arena,
                     const char *shader_dir) {
    char vertex_path[4096];
    char fragment_path[4096];
    float identity[16];

    if (renderer == NULL || arena == NULL || shader_dir == NULL) {
        return 0;
    }

    memset(renderer, 0, sizeof(*renderer));
    renderer->render_scale = 1.0f;

    snprintf(vertex_path, sizeof(vertex_path), "%s/basic.vert", shader_dir);
    snprintf(fragment_path, sizeof(fragment_path), "%s/basic.frag", shader_dir);

    if (!tc_shader_load(&renderer->shader, arena, vertex_path, fragment_path)) {
        return 0;
    }

    renderer->u_mvp = tc_shader_uniform(&renderer->shader, "u_mvp");
    renderer->u_texture = tc_shader_uniform(&renderer->shader, "u_texture");
    renderer->u_camera_pos = tc_shader_uniform(&renderer->shader,
                                               "u_camera_pos");
    renderer->u_fog_color = tc_shader_uniform(&renderer->shader,
                                              "u_fog_color");
    renderer->u_fog_start = tc_shader_uniform(&renderer->shader,
                                              "u_fog_start");
    renderer->u_fog_end = tc_shader_uniform(&renderer->shader, "u_fog_end");
    if (renderer->u_mvp < 0 || renderer->u_texture < 0
        || renderer->u_camera_pos < 0 || renderer->u_fog_color < 0
        || renderer->u_fog_start < 0 || renderer->u_fog_end < 0) {
        tc_log_error("shader is missing required uniforms");
        tc_renderer_shutdown(renderer);
        return 0;
    }

    create_atlas_texture(&renderer->texture);

    tc_shader_bind(&renderer->shader);
    glUniform1i(renderer->u_texture, 0);
    tc_mat4_identity(identity);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, identity);

    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    return 1;
}

int tc_renderer_upload_mesh(tc_renderer *renderer, tc_render_mesh *target,
                            const tc_mesh *mesh) {
    if (renderer == NULL || target == NULL || mesh == NULL) {
        return 0;
    }

    if (mesh->vertex_count > SIZE_MAX / sizeof(tc_vertex)
        || mesh->index_count > (size_t)INT_MAX) {
        return 0;
    }

    if (target->vao == 0) {
        glGenVertexArrays(1, &target->vao);
        glGenBuffers(1, &target->vbo);
        glGenBuffers(1, &target->ebo);
        configure_mesh(target);
    }

    if (mesh->vertex_count == 0 || mesh->index_count == 0) {
        target->index_count = 0;
        return 1;
    }

    glBindVertexArray(target->vao);

    {
        size_t vertex_bytes = mesh->vertex_count * sizeof(tc_vertex);
        size_t index_bytes = mesh->index_count * sizeof(uint32_t);

        glBindBuffer(GL_ARRAY_BUFFER, target->vbo);
        if (vertex_bytes > target->vbo_capacity) {
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertex_bytes,
                         mesh->vertices, GL_DYNAMIC_DRAW);
            target->vbo_capacity = vertex_bytes;
        } else {
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)target->vbo_capacity,
                         NULL, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vertex_bytes,
                            mesh->vertices);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, target->ebo);
        if (index_bytes > target->ebo_capacity) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)index_bytes,
                         mesh->indices, GL_DYNAMIC_DRAW);
            target->ebo_capacity = index_bytes;
        } else {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         (GLsizeiptr)target->ebo_capacity, NULL,
                         GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)index_bytes,
                            mesh->indices);
        }
    }

    glBindVertexArray(0);

    target->index_count = (unsigned int)mesh->index_count;
    return 1;
}

void tc_renderer_destroy_mesh(tc_render_mesh *mesh) {
    if (mesh == NULL) {
        return;
    }

    if (mesh->vao != 0) {
        glDeleteVertexArrays(1, &mesh->vao);
    }
    if (mesh->ebo != 0) {
        glDeleteBuffers(1, &mesh->ebo);
    }
    if (mesh->vbo != 0) {
        glDeleteBuffers(1, &mesh->vbo);
    }

    memset(mesh, 0, sizeof(*mesh));
}

void tc_renderer_set_render_scale(tc_renderer *renderer, float scale) {
    if (renderer == NULL) {
        return;
    }

    if (scale < 0.5f) {
        scale = 0.5f;
    } else if (scale > 1.0f) {
        scale = 1.0f;
    }

    renderer->render_scale = scale;
}

void tc_renderer_begin_frame(tc_renderer *renderer, const float eye[3],
                             const float target[3], int framebuffer_width,
                             int framebuffer_height) {
    const float sky_color[3] = {0.50f, 0.68f, 0.92f};
    float projection[16];
    float view[16];
    float mvp[16];
    float up[3] = {0.0f, 1.0f, 0.0f};
    float aspect;
    int render_width = framebuffer_width;
    int render_height = framebuffer_height;
    const float pi = 3.14159265358979323846f;

    if (renderer == NULL || eye == NULL || target == NULL
        || framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    renderer->window_width = framebuffer_width;
    renderer->window_height = framebuffer_height;
    renderer->use_scene_fbo = 0;

    if (renderer->render_scale < 0.999f) {
        render_width = (int)((float)framebuffer_width * renderer->render_scale
                             + 0.5f);
        render_height = (int)((float)framebuffer_height
                              * renderer->render_scale + 0.5f);
        if (render_width < 1) {
            render_width = 1;
        }
        if (render_height < 1) {
            render_height = 1;
        }

        if (render_width != framebuffer_width
            || render_height != framebuffer_height) {
            if (ensure_scene_target(renderer, render_width, render_height)) {
                glBindFramebuffer(GL_FRAMEBUFFER, renderer->scene_fbo);
                renderer->use_scene_fbo = 1;
            } else {
                render_width = framebuffer_width;
                render_height = framebuffer_height;
            }
        }
    }

    if (!renderer->use_scene_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, render_width, render_height);
    glClearColor(sky_color[0], sky_color[1], sky_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    aspect = (float)render_width / (float)render_height;
    tc_mat4_perspective(projection, TC_RENDER_FOV_DEGREES * pi / 180.0f,
                        aspect, TC_RENDER_NEAR_PLANE,
                        TC_RENDER_FAR_PLANE);
    tc_mat4_look_at(view, eye, target, up);
    tc_mat4_mul(mvp, projection, view);

    tc_shader_bind(&renderer->shader);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, mvp);
    glUniform3fv(renderer->u_camera_pos, 1, eye);
    glUniform3fv(renderer->u_fog_color, 1, sky_color);
    glUniform1f(renderer->u_fog_start, 48.0f);
    glUniform1f(renderer->u_fog_end, 80.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->texture);
}

void tc_renderer_end_frame(tc_renderer *renderer) {
    if (renderer == NULL) {
        return;
    }

    if (renderer->use_scene_fbo) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->scene_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, renderer->scene_width, renderer->scene_height,
                          0, 0, renderer->window_width,
                          renderer->window_height,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, renderer->window_width, renderer->window_height);
    renderer->use_scene_fbo = 0;
}

void tc_renderer_draw_mesh(tc_renderer *renderer, const tc_render_mesh *mesh) {
    if (renderer == NULL || mesh == NULL || mesh->vao == 0
        || mesh->index_count == 0) {
        return;
    }

    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->index_count,
                   GL_UNSIGNED_INT, (void *)0);
    glBindVertexArray(0);
}

void tc_renderer_shutdown(tc_renderer *renderer) {
    if (renderer == NULL) {
        return;
    }

    destroy_scene_target(renderer);
    if (renderer->texture != 0) {
        glDeleteTextures(1, &renderer->texture);
    }
    tc_shader_destroy(&renderer->shader);
    memset(renderer, 0, sizeof(*renderer));
}
