#ifndef THINKCRAFT_MATH_H
#define THINKCRAFT_MATH_H

void tc_mat4_identity(float *out);
void tc_mat4_perspective(float *out, float fov_y_radians, float aspect,
                         float near_plane, float far_plane);
void tc_mat4_translate(float *out, float x, float y, float z);
void tc_mat4_rotate_x(float *out, float radians);
void tc_mat4_rotate_y(float *out, float radians);
void tc_mat4_look_at(float *out, const float eye[3], const float center[3],
                     const float up[3]);
void tc_mat4_mul(float *out, const float *a, const float *b);

#endif
