#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

JArray *rt_new_float_array(uint32_t length) {
    size_t bytes;

    if (length > (UINT32_MAX - sizeof(JArray)) / sizeof(float)) {
        fprintf(stderr, "rt_new_float_array: array too large\n");
        abort();
    }

    bytes = sizeof(JArray) + (size_t)length * sizeof(float);
    {
        JArray *array = rt_alloc(bytes);
        array->length = length;
        return array;
    }
}
