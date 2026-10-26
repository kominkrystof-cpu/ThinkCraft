#include "thinkcraft/math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void mat4_mul_reference(float *out, const float *a, const float *b) {
    float result[16];

    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            float value = 0.0f;
            for (int k = 0; k < 4; k++) {
                value += a[k * 4 + row] * b[column * 4 + k];
            }
            result[column * 4 + row] = value;
        }
    }

    memcpy(out, result, sizeof(result));
}

static int compare_matrices(const char *name, const float *actual,
                            const float *expected) {
    for (int i = 0; i < 16; i++) {
        float difference = fabsf(actual[i] - expected[i]);
        if (difference > 1e-5f) {
            fprintf(stderr, "%s: element %d differs: %.8f vs %.8f\n",
                    name, i, actual[i], expected[i]);
            return 0;
        }
    }
    return 1;
}

static int test_identity(void) {
    float identity[16];
    float a[16];
    float result[16];
    float expected[16];

    tc_mat4_identity(identity);
    for (int i = 0; i < 16; i++) {
        a[i] = (float)(i + 1) * 0.37f;
        expected[i] = a[i];
    }

    tc_mat4_mul(result, a, identity);
    if (!compare_matrices("identity right", result, expected)) {
        return 0;
    }

    tc_mat4_mul(result, identity, a);
    if (!compare_matrices("identity left", result, expected)) {
        return 0;
    }

    return 1;
}

static int test_general_multiplication(void) {
    const float a[16] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    };
    const float b[16] = {
        -2.0f, 5.0f, 0.5f, 1.0f,
        3.0f, -1.0f, 2.0f, 0.0f,
        7.0f, 0.25f, -3.0f, 2.0f,
        1.5f, 4.0f, 6.0f, -1.0f
    };
    float actual[16];
    float expected[16];

    tc_mat4_mul(actual, a, b);
    mat4_mul_reference(expected, a, b);
    return compare_matrices("general", actual, expected);
}

static int test_transform_composition(void) {
    float translate[16];
    float rotate_x[16];
    float rotate_y[16];
    float model[16];
    float actual[16];
    float expected[16];
    float projection_view[16];
    float view[16];

    tc_mat4_translate(translate, 1.5f, -2.0f, 3.25f);
    tc_mat4_rotate_x(rotate_x, 0.37f);
    tc_mat4_rotate_y(rotate_y, -1.2f);
    tc_mat4_translate(view, 0.0f, 0.0f, -3.5f);

    tc_mat4_mul(model, rotate_x, rotate_y);
    tc_mat4_mul(actual, translate, model);
    mat4_mul_reference(expected, translate, model);
    if (!compare_matrices("transform", actual, expected)) {
        return 0;
    }

    tc_mat4_mul(projection_view, view, model);
    mat4_mul_reference(expected, view, model);
    return compare_matrices("view model", projection_view, expected);
}

static void transform_point(float out[3], const float matrix[16],
                            const float point[3]) {
    out[0] = matrix[0] * point[0] + matrix[4] * point[1]
           + matrix[8] * point[2] + matrix[12];
    out[1] = matrix[1] * point[0] + matrix[5] * point[1]
           + matrix[9] * point[2] + matrix[13];
    out[2] = matrix[2] * point[0] + matrix[6] * point[1]
           + matrix[10] * point[2] + matrix[14];
}

static int test_look_at(void) {
    const float eye[3] = {0.0f, 0.0f, 3.0f};
    const float center[3] = {0.0f, 0.0f, 0.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    float view[16];
    float transformed_eye[3];
    float transformed_origin[3];

    tc_mat4_look_at(view, eye, center, up);
    transform_point(transformed_eye, view, eye);
    transform_point(transformed_origin, view, center);

    if (fabsf(transformed_eye[0]) > 1e-5f
        || fabsf(transformed_eye[1]) > 1e-5f
        || fabsf(transformed_eye[2]) > 1e-5f) {
        fprintf(stderr, "look_at: camera position did not map to origin\n");
        return 0;
    }

    if (fabsf(transformed_origin[2] + 3.0f) > 1e-5f) {
        fprintf(stderr, "look_at: wrong forward distance\n");
        return 0;
    }

    return 1;
}

static int test_perspective(void) {
    float matrix[16];
    float fov = 1.0471975512f;
    float aspect = 16.0f / 9.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    float expected_f;

    tc_mat4_perspective(matrix, fov, aspect, near_plane, far_plane);
    expected_f = 1.0f / tanf(fov * 0.5f);

    if (fabsf(matrix[0] - expected_f / aspect) > 1e-5f) {
        fprintf(stderr, "perspective: wrong x scale\n");
        return 0;
    }
    if (fabsf(matrix[5] - expected_f) > 1e-5f) {
        fprintf(stderr, "perspective: wrong y scale\n");
        return 0;
    }
    if (fabsf(matrix[11] + 1.0f) > 1e-5f) {
        fprintf(stderr, "perspective: wrong w component\n");
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_identity()
        || !test_general_multiplication()
        || !test_transform_composition()
        || !test_look_at()
        || !test_perspective()) {
        return 1;
    }

    printf("test_math: all checks passed\n");
    return 0;
}
