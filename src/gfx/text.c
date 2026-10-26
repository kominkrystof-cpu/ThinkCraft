#include "thinkcraft/text.h"
#include "thinkcraft/log.h"

#include <glad/gl.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TC_TEXT_GLYPH_COUNT 64
#define TC_TEXT_COLUMNS 8
#define TC_TEXT_CELL_SIZE 8
#define TC_TEXT_ATLAS_SIZE (TC_TEXT_COLUMNS * TC_TEXT_CELL_SIZE)
#define TC_TEXT_MAX_CHARS 256
#define TC_TEXT_FLOATS_PER_VERTEX 4
#define TC_TEXT_VERTICES_PER_CHAR 6
#define TC_TEXT_VBO_FLOATS \
    (TC_TEXT_MAX_CHARS * TC_TEXT_VERTICES_PER_CHAR * TC_TEXT_FLOATS_PER_VERTEX)

static const uint8_t font5x7[TC_TEXT_GLYPH_COUNT][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f, 0x00, 0x00}, {0x00, 0x03, 0x00, 0x03, 0x00}, {0x12, 0x3f, 0x12, 0x3f, 0x12},
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, {0x43, 0x33, 0x0c, 0x62, 0x61}, {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x00, 0x03, 0x00, 0x00},
    {0x00, 0x1c, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1c, 0x00}, {0x2a, 0x1c, 0x3e, 0x1c, 0x2a}, {0x08, 0x08, 0x3e, 0x08, 0x08},
    {0x00, 0x40, 0x30, 0x10, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x00, 0x60, 0x60, 0x00}, {0x40, 0x30, 0x08, 0x06, 0x01},
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46}, {0x41, 0x49, 0x49, 0x49, 0x36},
    {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x4f, 0x49, 0x49, 0x49, 0x31}, {0x3e, 0x49, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x49, 0x3e}, {0x00, 0x00, 0x12, 0x00, 0x00}, {0x00, 0x40, 0x32, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14}, {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x3e, 0x41, 0x5d, 0x55, 0x1e}, {0x7e, 0x09, 0x09, 0x09, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x3a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x41, 0x41, 0x7f, 0x41, 0x41}, {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x02, 0x04, 0x08, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7f, 0x01, 0x01}, {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7f, 0x41, 0x41, 0x00},
    {0x01, 0x06, 0x08, 0x30, 0x40}, {0x00, 0x41, 0x41, 0x7f, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40}
};

static int glyph_index(unsigned char character) {
    if (character >= 'a' && character <= 'z') {
        character = (unsigned char)(character - 'a' + 'A');
    }
    if (character < 32 || character > 95) {
        character = '?';
    }
    return (int)(character - 32);
}

static void generate_font_pixels(uint8_t *pixels) {
    memset(pixels, 0, TC_TEXT_ATLAS_SIZE * TC_TEXT_ATLAS_SIZE);

    for (int glyph = 0; glyph < TC_TEXT_GLYPH_COUNT; glyph++) {
        int cell_x = (glyph % TC_TEXT_COLUMNS) * TC_TEXT_CELL_SIZE;
        int cell_y = (glyph / TC_TEXT_COLUMNS) * TC_TEXT_CELL_SIZE;

        for (int column = 0; column < 5; column++) {
            uint8_t bits = font5x7[glyph][column];

            for (int row = 0; row < 7; row++) {
                if ((bits & (1u << row)) == 0) {
                    continue;
                }
                pixels[(size_t)(cell_y + 1 + row) * TC_TEXT_ATLAS_SIZE
                       + (size_t)(cell_x + 1 + column)] = 255;
            }
        }
    }
}

static int create_font_texture(unsigned int *texture) {
    uint8_t pixels[TC_TEXT_ATLAS_SIZE * TC_TEXT_ATLAS_SIZE];

    generate_font_pixels(pixels);

    glGenTextures(1, texture);
    if (*texture == 0) {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, *texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, TC_TEXT_ATLAS_SIZE,
                 TC_TEXT_ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return 1;
}

int tc_text_init(tc_text_renderer *text, tc_arena *arena,
                 const char *shader_dir) {
    char vertex_path[4096];
    char fragment_path[4096];

    if (text == NULL || arena == NULL || shader_dir == NULL) {
        return 0;
    }

    memset(text, 0, sizeof(*text));

    snprintf(vertex_path, sizeof(vertex_path), "%s/text.vert", shader_dir);
    snprintf(fragment_path, sizeof(fragment_path), "%s/text.frag", shader_dir);

    if (!tc_shader_load(&text->shader, arena, vertex_path, fragment_path)) {
        return 0;
    }

    text->u_viewport = tc_shader_uniform(&text->shader, "u_viewport");
    text->u_color = tc_shader_uniform(&text->shader, "u_color");
    text->u_font = tc_shader_uniform(&text->shader, "u_font");
    if (text->u_viewport < 0 || text->u_color < 0 || text->u_font < 0) {
        tc_log_error("text shader is missing required uniforms");
        tc_text_shutdown(text);
        return 0;
    }

    if (!create_font_texture(&text->texture)) {
        tc_text_shutdown(text);
        return 0;
    }

    glGenVertexArrays(1, &text->vao);
    glGenBuffers(1, &text->vbo);
    if (text->vao == 0 || text->vbo == 0) {
        tc_text_shutdown(text);
        return 0;
    }

    glBindVertexArray(text->vao);
    glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
    glBufferData(GL_ARRAY_BUFFER, TC_TEXT_VBO_FLOATS * sizeof(float), NULL,
                 GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          TC_TEXT_FLOATS_PER_VERTEX * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          TC_TEXT_FLOATS_PER_VERTEX * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    tc_shader_bind(&text->shader);
    glUniform1i(text->u_font, 0);

    text->initialized = 1;
    return 1;
}

void tc_text_shutdown(tc_text_renderer *text) {
    if (text == NULL) {
        return;
    }

    if (text->vbo != 0) {
        glDeleteBuffers(1, &text->vbo);
    }
    if (text->vao != 0) {
        glDeleteVertexArrays(1, &text->vao);
    }
    if (text->texture != 0) {
        glDeleteTextures(1, &text->texture);
    }
    tc_shader_destroy(&text->shader);
    memset(text, 0, sizeof(*text));
}

void tc_text_begin(tc_text_renderer *text, int framebuffer_width,
                   int framebuffer_height) {
    if (text == NULL || !text->initialized || framebuffer_width <= 0
        || framebuffer_height <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, framebuffer_width, framebuffer_height);

    tc_shader_bind(&text->shader);
    glUniform2f(text->u_viewport, (float)framebuffer_width,
                (float)framebuffer_height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, text->texture);
    glBindVertexArray(text->vao);
    glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
}

void tc_text_draw(tc_text_renderer *text, const char *string, int x, int y,
                  float scale, float red, float green, float blue) {
    float vertices[TC_TEXT_VBO_FLOATS];
    float cursor = (float)x;
    size_t vertex_count = 0;
    size_t length;

    if (text == NULL || !text->initialized || string == NULL || scale <= 0.0f) {
        return;
    }

    length = strlen(string);
    if (length > TC_TEXT_MAX_CHARS) {
        length = TC_TEXT_MAX_CHARS;
    }

    for (size_t index = 0; index < length; index++) {
        int glyph = glyph_index((unsigned char)string[index]);
        int cell_x = (glyph % TC_TEXT_COLUMNS) * TC_TEXT_CELL_SIZE;
        int cell_y = (glyph / TC_TEXT_COLUMNS) * TC_TEXT_CELL_SIZE;
        float u0 = (float)cell_x / (float)TC_TEXT_ATLAS_SIZE;
        float v0 = (float)cell_y / (float)TC_TEXT_ATLAS_SIZE;
        float u1 = (float)(cell_x + TC_TEXT_CELL_SIZE)
                 / (float)TC_TEXT_ATLAS_SIZE;
        float v1 = (float)(cell_y + TC_TEXT_CELL_SIZE)
                 / (float)TC_TEXT_ATLAS_SIZE;
        float x0 = cursor;
        float y0 = (float)y;
        float x1 = cursor + (float)TC_TEXT_CELL_SIZE * scale;
        float y1 = (float)y + (float)TC_TEXT_CELL_SIZE * scale;
        float *vertex = &vertices[vertex_count * TC_TEXT_FLOATS_PER_VERTEX];

        vertex[0] = x0;
        vertex[1] = y0;
        vertex[2] = u0;
        vertex[3] = v0;
        vertex += TC_TEXT_FLOATS_PER_VERTEX;
        vertex[0] = x1;
        vertex[1] = y0;
        vertex[2] = u1;
        vertex[3] = v0;
        vertex += TC_TEXT_FLOATS_PER_VERTEX;
        vertex[0] = x1;
        vertex[1] = y1;
        vertex[2] = u1;
        vertex[3] = v1;
        vertex += TC_TEXT_FLOATS_PER_VERTEX;
        vertex[0] = x0;
        vertex[1] = y0;
        vertex[2] = u0;
        vertex[3] = v0;
        vertex += TC_TEXT_FLOATS_PER_VERTEX;
        vertex[0] = x1;
        vertex[1] = y1;
        vertex[2] = u1;
        vertex[3] = v1;
        vertex += TC_TEXT_FLOATS_PER_VERTEX;
        vertex[0] = x0;
        vertex[1] = y1;
        vertex[2] = u0;
        vertex[3] = v1;

        vertex_count += TC_TEXT_VERTICES_PER_CHAR;
        cursor += 6.0f * scale;
    }

    if (vertex_count == 0) {
        return;
    }

    glUniform3f(text->u_color, red, green, blue);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(vertex_count * TC_TEXT_FLOATS_PER_VERTEX
                                 * sizeof(float)), vertices);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
}

void tc_text_end(tc_text_renderer *text) {
    if (text == NULL || !text->initialized) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

int tc_text_width(const char *string, float scale) {
    size_t length;

    if (string == NULL || scale <= 0.0f) {
        return 0;
    }

    length = strlen(string);
    if (length > TC_TEXT_MAX_CHARS) {
        length = TC_TEXT_MAX_CHARS;
    }
    if (length == 0) {
        return 0;
    }

    return (int)(((float)length * 6.0f - 1.0f) * scale);
}
