#include "thinkcraft/math.h"

#include <math.h>
#include <string.h>

void tc_mat4_identity(float *out) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void tc_mat4_perspective(float *out, float fov_y_radians, float aspect,
                         float near_plane, float far_plane) {
    float f = 1.0f / tanf(fov_y_radians * 0.5f);

    memset(out, 0, sizeof(float) * 16);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (far_plane + near_plane) / (near_plane - far_plane);
    out[11] = -1.0f;
    out[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
}

void tc_mat4_translate(float *out, float x, float y, float z) {
    tc_mat4_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void tc_mat4_rotate_x(float *out, float radians) {
    float c = cosf(radians);
    float s = sinf(radians);

    tc_mat4_identity(out);
    out[5] = c;
    out[6] = s;
    out[9] = -s;
    out[10] = c;
}

void tc_mat4_rotate_y(float *out, float radians) {
    float c = cosf(radians);
    float s = sinf(radians);

    tc_mat4_identity(out);
    out[0] = c;
    out[2] = -s;
    out[8] = s;
    out[10] = c;
}
