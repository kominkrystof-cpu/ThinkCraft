#include "ir.h"

const char *vtype_name(VType type) {
    switch (type) {
    case VT_INT: return "int";
    case VT_FLOAT: return "float";
    case VT_REF: return "ref";
    case VT_VOID: return "void";
    case VT_LONG: return "long";
    case VT_DOUBLE: return "double";
    default: return "unsupported";
    }
}

int vtype_size(VType type) {
    switch (type) {
    case VT_INT:
    case VT_FLOAT:
        return 4;
    case VT_REF:
    case VT_LONG:
    case VT_DOUBLE:
        return 8;
    default:
        return 0;
    }
}
