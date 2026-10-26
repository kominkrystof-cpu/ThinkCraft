#include "thinkcraft/renderer.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"

#include <glad/gl.h>

#include <limits.h>
#include <math.h>
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
    if (renderer->u_mvp < 0 || renderer->u_texture < 0) {
        tc_log_error("shader is missing required uniforms");
        tc_renderer_shutdown(renderer);
        return 0;
    }

    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ebo);

    glBindVertexArray(renderer->vao);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(tc_vertex),
                          (void *)(uintptr_t)offsetof(tc_vertex, color));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
    glBindVertexArray(0);

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

int tc_renderer_upload_mesh(tc_renderer *renderer, const tc_mesh *mesh) {
    if (renderer == NULL || mesh == NULL) {
        return 0;
    }

    if (mesh->vertex_count == 0 || mesh->index_count == 0) {
        return 0;
    }

    if (mesh->vertex_count > SIZE_MAX / sizeof(tc_vertex)
        || mesh->index_count > (size_t)INT_MAX) {
        return 0;
    }

    glBindVertexArray(renderer->vao);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->vertex_count * sizeof(tc_vertex)),
                 mesh->vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)),
                 mesh->indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    renderer->index_count = (unsigned int)mesh->index_count;
    return 1;
}

void tc_renderer_shutdown(tc_renderer *renderer) {
    if (renderer == NULL) {
        return;
    }

    if (renderer->texture != 0) {
        glDeleteTextures(1, &renderer->texture);
    }
    if (renderer->ebo != 0) {
        glDeleteBuffers(1, &renderer->ebo);
    }
    if (renderer->vbo != 0) {
        glDeleteBuffers(1, &renderer->vbo);
    }
    if (renderer->vao != 0) {
        glDeleteVertexArrays(1, &renderer->vao);
    }
    tc_shader_destroy(&renderer->shader);
    memset(renderer, 0, sizeof(*renderer));
}

void tc_renderer_frame(tc_renderer *renderer, double time_seconds,
                       int framebuffer_width, int framebuffer_height) {
    float projection[16];
    float view[16];
    float mvp[16];
    float aspect;
    float angle;
    float eye[3];
    float target[3] = {8.0f, 10.0f, 8.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    const float pi = 3.14159265358979323846f;

    if (renderer == NULL || framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    angle = (float)time_seconds * 0.18f;
    eye[0] = target[0] + sinf(angle) * 34.0f;
    eye[1] = 20.0f;
    eye[2] = target[2] + cosf(angle) * 34.0f;

    aspect = (float)framebuffer_width / (float)framebuffer_height;
    tc_mat4_perspective(projection, 60.0f * pi / 180.0f, aspect,
                        0.1f, 256.0f);
    tc_mat4_look_at(view, eye, target, up);
    tc_mat4_mul(mvp, projection, view);

    tc_shader_bind(&renderer->shader);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, mvp);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glBindVertexArray(renderer->vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)renderer->index_count,
                   GL_UNSIGNED_INT, (void *)0);
    glBindVertexArray(0);
}
