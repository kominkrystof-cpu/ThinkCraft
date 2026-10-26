#include "thinkcraft/renderer.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"

#include <glad/gl.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void create_white_texture(unsigned int *texture) {
    const unsigned char pixel[4] = {255, 255, 255, 255};

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixel);
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
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, color));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBindVertexArray(0);
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

    create_white_texture(&renderer->texture);

    tc_shader_bind(&renderer->shader);
    glUniform1i(renderer->u_texture, 0);
    tc_mat4_identity(identity);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, identity);

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

    glBindBuffer(GL_ARRAY_BUFFER, target->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->vertex_count * sizeof(tc_vertex)),
                 mesh->vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, target->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)),
                 mesh->indices, GL_STATIC_DRAW);

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

void tc_renderer_begin_frame(tc_renderer *renderer, const float eye[3],
                             const float target[3], int framebuffer_width,
                             int framebuffer_height) {
    const float sky_color[3] = {0.50f, 0.68f, 0.92f};
    float projection[16];
    float view[16];
    float mvp[16];
    float up[3] = {0.0f, 1.0f, 0.0f};
    float aspect;
    const float pi = 3.14159265358979323846f;

    if (renderer == NULL || eye == NULL || target == NULL
        || framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(sky_color[0], sky_color[1], sky_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    aspect = (float)framebuffer_width / (float)framebuffer_height;
    tc_mat4_perspective(projection, 60.0f * pi / 180.0f, aspect,
                        0.1f, 256.0f);
    tc_mat4_look_at(view, eye, target, up);
    tc_mat4_mul(mvp, projection, view);

    tc_shader_bind(&renderer->shader);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, mvp);
    glUniform3fv(renderer->u_camera_pos, 1, eye);
    glUniform3fv(renderer->u_fog_color, 1, sky_color);
    glUniform1f(renderer->u_fog_start, 34.0f);
    glUniform1f(renderer->u_fog_end, 56.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
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

    if (renderer->texture != 0) {
        glDeleteTextures(1, &renderer->texture);
    }
    tc_shader_destroy(&renderer->shader);
    memset(renderer, 0, sizeof(*renderer));
}
