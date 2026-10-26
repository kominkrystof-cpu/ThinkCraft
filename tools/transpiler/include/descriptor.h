#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include "ir.h"

#define MAX_METHOD_ARGS 16

typedef struct {
    VType args[MAX_METHOD_ARGS];
    int nargs;
    VType ret;
    int supported;
} MethodDesc;

int descriptor_parse_method(const char *descriptor, MethodDesc *out);
int descriptor_parse_field(const char *descriptor, VType *out);

#endif
