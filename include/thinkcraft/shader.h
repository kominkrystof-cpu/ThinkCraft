#ifndef THINKCRAFT_SHADER_H
#define THINKCRAFT_SHADER_H

#include "thinkcraft/arena.h"

typedef struct {
    unsigned int program;
} tc_shader;

int tc_shader_load(tc_shader *shader, tc_arena *arena,
                   const char *vertex_path, const char *fragment_path);
void tc_shader_destroy(tc_shader *shader);
void tc_shader_bind(const tc_shader *shader);
int tc_shader_uniform(const tc_shader *shader, const char *name);

#endif
