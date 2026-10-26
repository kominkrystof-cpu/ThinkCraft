#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stddef.h>
#include "classfile.h"
#include "descriptor.h"

int opcode_length(const uint8_t *code, uint32_t code_length, uint32_t pc, uint32_t *out_length);
int bytecode_check_method(const ClassFile *cf, const Method *method, const MethodDesc *desc,
                          char *error, size_t error_size);

#endif
