#ifndef EMIT_NASM_H
#define EMIT_NASM_H

#include "classfile.h"

int emit_nasm(ClassFile *cf, const char *out_path, const char *prefix, int verbose);

#endif
