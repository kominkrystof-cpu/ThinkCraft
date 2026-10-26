#include "runtime.h"

#include <stdio.h>

typedef struct JObject JObject;

extern JObject *cva_new(int count);
extern int cva_size(JObject *self);
extern void cva_set_i_f_f_f(JObject *self, int index, float x, float y, float z);
extern float cva_getX_i(JObject *self, int index);
extern float cva_getY_i(JObject *self, int index);
extern float cva_getZ_i(JObject *self, int index);

static int expect_int(const char *what, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "FAIL %s: expected %d, got %d\n", what, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_float(const char *what, float actual, float expected) {
    if (actual != expected) {
        fprintf(stderr, "FAIL %s: expected %f, got %f\n", what, expected, actual);
        return 1;
    }
    return 0;
}

int main(void) {
    JObject *vectors;
    int failures = 0;

    rt_arena_init(1024 * 1024);
    vectors = cva_new(4);

    failures += expect_int("size", cva_size(vectors), 4);

    cva_set_i_f_f_f(vectors, 0, 1.25f, -2.5f, 3.75f);
    cva_set_i_f_f_f(vectors, 3, 10.5f, 20.25f, -30.125f);

    failures += expect_float("getX(0)", cva_getX_i(vectors, 0), 1.25f);
    failures += expect_float("getY(0)", cva_getY_i(vectors, 0), -2.5f);
    failures += expect_float("getZ(0)", cva_getZ_i(vectors, 0), 3.75f);

    failures += expect_float("getX(3)", cva_getX_i(vectors, 3), 10.5f);
    failures += expect_float("getY(3)", cva_getY_i(vectors, 3), 20.25f);
    failures += expect_float("getZ(3)", cva_getZ_i(vectors, 3), -30.125f);

    if (failures != 0) {
        return 1;
    }

    printf("CompactVectorArray: all checks passed\n");
    return 0;
}
