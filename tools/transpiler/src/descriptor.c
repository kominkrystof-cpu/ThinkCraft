#include "descriptor.h"

#include <string.h>

static const char *parse_type(const char *p, VType *out) {
    if (p == NULL || *p == '\0') {
        return NULL;
    }

    switch (*p) {
    case 'I':
    case 'Z':
    case 'B':
    case 'C':
    case 'S':
        *out = VT_INT;
        return p + 1;
    case 'F':
        *out = VT_FLOAT;
        return p + 1;
    case 'J':
        *out = VT_LONG;
        return p + 1;
    case 'D':
        *out = VT_DOUBLE;
        return p + 1;
    case 'V':
        *out = VT_VOID;
        return p + 1;
    case 'L': {
        const char *semi = strchr(p, ';');
        if (semi == NULL) {
            return NULL;
        }
        *out = VT_REF;
        return semi + 1;
    }
    case '[': {
        const char *q = p;
        VType element;
        while (*q == '[') {
            q++;
        }
        q = parse_type(q, &element);
        if (q == NULL) {
            return NULL;
        }
        *out = VT_REF;
        return q;
    }
    default:
        return NULL;
    }
}

int descriptor_parse_method(const char *descriptor, MethodDesc *out) {
    const char *p;

    if (descriptor == NULL || out == NULL) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->ret = VT_VOID;
    out->supported = 1;

    if (*descriptor != '(') {
        out->supported = 0;
        return 0;
    }

    p = descriptor + 1;
    while (*p != '\0' && *p != ')') {
        VType type;
        p = parse_type(p, &type);
        if (p == NULL) {
            out->supported = 0;
            return 0;
        }
        if (out->nargs >= MAX_METHOD_ARGS) {
            out->supported = 0;
            return 0;
        }
        out->args[out->nargs++] = type;
        if (type != VT_INT && type != VT_FLOAT) {
            out->supported = 0;
        }
    }

    if (*p != ')') {
        out->supported = 0;
        return 0;
    }
    p++;

    {
        VType ret;
        const char *end = parse_type(p, &ret);
        if (end == NULL || *end != '\0') {
            out->supported = 0;
            return 0;
        }
        out->ret = ret;
        if (ret != VT_INT && ret != VT_FLOAT && ret != VT_VOID) {
            out->supported = 0;
        }
    }

    return out->supported;
}

int descriptor_parse_field(const char *descriptor, VType *out) {
    VType type;
    const char *end;

    if (descriptor == NULL || out == NULL) {
        return 0;
    }

    end = parse_type(descriptor, &type);
    if (end == NULL || *end != '\0') {
        return 0;
    }

    *out = type;
    return 1;
}
