#include "thinkcraft/renderer.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"

#include <glad/gl.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const float cube_vertices[] = {
    -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
     0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
     0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f, 0.0f, 1.0f,

     0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
     0.5f,  0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,

     0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
     0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f, 0.0f, 1.0f,

    -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
     0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
     0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f, 1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f, 0.0f, 1.0f
};

static const unsigned int cube_indices[] = {
     0,  1,  2,  0,  2,  3,
     4,  5,  6,  4,  6,  7,
     8,  9, 10,  8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23
};

static void create_checker_texture(unsigned int *texture) {
    unsigned char pixels[16 * 16 * 4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int checker = ((x / 4) + (y / 4)) & 1;
            int offset = (y * 16 + x) * 4;
            pixels[offset + 0] = checker ? 150 : 78;
            pixels[offset + 1] = checker ? 112 : 132;
            pixels[offset + 2] = checker ? 64 : 54;
            pixels[offset + 3] = 255;
        }
    }

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

int tc_renderer_init(tc_renderer *renderer, tc_arena *arena, const char *shader_dir) {
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void *)(uintptr_t)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    create_checker_texture(&renderer->texture);

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
    float rotate_x[16];
    float rotate_y[16];
    float model[16];
    float projection_view[16];
    float mvp[16];
    float aspect;
    const float pi = 3.14159265358979323846f;

    if (renderer == NULL || framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    aspect = (float)framebuffer_width / (float)framebuffer_height;
    tc_mat4_perspective(projection, 60.0f * pi / 180.0f, aspect, 0.1f, 100.0f);
    tc_mat4_translate(view, 0.0f, 0.0f, -3.5f);
    tc_mat4_rotate_x(rotate_x, (float)time_seconds * 0.45f);
    tc_mat4_rotate_y(rotate_y, (float)time_seconds * 0.70f);
    tc_mat4_mul(model, rotate_x, rotate_y);
    tc_mat4_mul(projection_view, projection, view);
    tc_mat4_mul(mvp, projection_view, model);

    tc_shader_bind(&renderer->shader);
    glUniformMatrix4fv(renderer->u_mvp, 1, GL_FALSE, mvp);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glBindVertexArray(renderer->vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0);
    glBindVertexArray(0);
}
