#include "thinkcraft/math.h"

#include <math.h>
#include <string.h>

static float dot3(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void cross3(float out[3], const float a[3], const float b[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void normalize3(float value[3]) {
    float length = sqrtf(dot3(value, value));

    if (length > 0.0f) {
        value[0] /= length;
        value[1] /= length;
        value[2] /= length;
    }
}

int tc_math_floor_div(int value, int divisor) {
    int quotient;
    int remainder;

    if (divisor == 0) {
        return 0;
    }

    quotient = value / divisor;
    remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        quotient--;
    }

    return quotient;
}

int tc_math_floor_mod(int value, int divisor) {
    if (divisor == 0) {
        return 0;
    }

    return value - tc_math_floor_div(value, divisor) * divisor;
}

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

void tc_mat4_look_at(float *out, const float eye[3], const float center[3],
                     const float up[3]) {
    float forward[3] = {
        center[0] - eye[0],
        center[1] - eye[1],
        center[2] - eye[2]
    };
    float side[3];
    float corrected_up[3];

    normalize3(forward);
    cross3(side, forward, up);
    normalize3(side);
    cross3(corrected_up, side, forward);

    out[0] = side[0];
    out[4] = side[1];
    out[8] = side[2];
    out[12] = -dot3(side, eye);
    out[1] = corrected_up[0];
    out[5] = corrected_up[1];
    out[9] = corrected_up[2];
    out[13] = -dot3(corrected_up, eye);
    out[2] = -forward[0];
    out[6] = -forward[1];
    out[10] = -forward[2];
    out[14] = dot3(forward, eye);
    out[3] = 0.0f;
    out[7] = 0.0f;
    out[11] = 0.0f;
    out[15] = 1.0f;
}
