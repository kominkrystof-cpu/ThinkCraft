#include "thinkcraft/shader.h"
#include "thinkcraft/log.h"
#include "thinkcraft/platform.h"

#include <glad/gl.h>

#include <stdio.h>

static GLuint compile_shader(GLenum type, const char *source, const char *label) {
    GLuint shader = glCreateShader(type);
    GLint status = GL_FALSE;
    GLint log_length = 0;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        char log[4096];
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length >= (GLint)sizeof(log)) {
            log_length = (GLint)sizeof(log) - 1;
        }
        glGetShaderInfoLog(shader, log_length, NULL, log);
        log[log_length] = '\0';
        tc_log_error("failed to compile %s shader:\n%s", label, log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_program(GLuint vertex, GLuint fragment) {
    GLuint program = glCreateProgram();
    GLint status = GL_FALSE;
    GLint log_length = 0;

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        char log[4096];
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length >= (GLint)sizeof(log)) {
            log_length = (GLint)sizeof(log) - 1;
        }
        glGetProgramInfoLog(program, log_length, NULL, log);
        log[log_length] = '\0';
        tc_log_error("failed to link shader program:\n%s", log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

int tc_shader_load(tc_shader *shader, tc_arena *arena,
                   const char *vertex_path, const char *fragment_path) {
    char *vertex_source;
    char *fragment_source;
    GLuint vertex;
    GLuint fragment;
    GLuint program;

    if (shader == NULL || arena == NULL) {
        return 0;
    }

    vertex_source = tc_read_text_file(vertex_path, arena);
    fragment_source = tc_read_text_file(fragment_path, arena);
    if (vertex_source == NULL || fragment_source == NULL) {
        return 0;
    }

    vertex = compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex");
    fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source, "fragment");
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return 0;
    }

    program = link_program(vertex, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    if (program == 0) {
        return 0;
    }

    shader->program = program;
    return 1;
}

void tc_shader_destroy(tc_shader *shader) {
    if (shader != NULL && shader->program != 0) {
        glDeleteProgram(shader->program);
        shader->program = 0;
    }
}

void tc_shader_bind(const tc_shader *shader) {
    if (shader != NULL) {
        glUseProgram(shader->program);
    }
}

int tc_shader_uniform(const tc_shader *shader, const char *name) {
    if (shader == NULL) {
        return -1;
    }
    return glGetUniformLocation(shader->program, name);
}
