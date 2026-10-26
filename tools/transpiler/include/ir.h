#ifndef IR_H
#define IR_H

typedef enum {
    VT_INT,
    VT_FLOAT,
    VT_REF,
    VT_VOID,
    VT_LONG,
    VT_DOUBLE,
    VT_UNSUPPORTED
} VType;

const char *vtype_name(VType type);
int vtype_size(VType type);

#endif
