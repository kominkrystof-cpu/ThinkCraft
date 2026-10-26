#include "bytecode.h"

#include <stdio.h>
#include <string.h>

static int32_t read_s4(const uint8_t *code, uint32_t offset) {
    return (int32_t)(((uint32_t)code[offset] << 24)
                   | ((uint32_t)code[offset + 1] << 16)
                   | ((uint32_t)code[offset + 2] << 8)
                   | (uint32_t)code[offset + 3]);
}

int opcode_length(const uint8_t *code, uint32_t code_length, uint32_t pc, uint32_t *out_length) {
    uint8_t op;
    uint32_t length = 0;

    if (code == NULL || out_length == NULL || pc >= code_length) {
        return 0;
    }

    op = code[pc];

    if (op <= 0x0f) {
        length = 1;
    } else if (op == 0x10) {
        length = 2;
    } else if (op == 0x11) {
        length = 3;
    } else if (op == 0x12) {
        length = 2;
    } else if (op == 0x13 || op == 0x14) {
        length = 3;
    } else if (op >= 0x15 && op <= 0x19) {
        length = 2;
    } else if (op >= 0x1a && op <= 0x35) {
        length = 1;
    } else if (op >= 0x36 && op <= 0x3a) {
        length = 2;
    } else if (op >= 0x3b && op <= 0x83) {
        length = 1;
    } else if (op == 0x84) {
        length = 3;
    } else if (op >= 0x85 && op <= 0x98) {
        length = 1;
    } else if (op >= 0x99 && op <= 0xa8) {
        length = 3;
    } else if (op == 0xa9) {
        length = 2;
    } else if (op == 0xaa || op == 0xab) {
        uint32_t padding = (4 - ((pc + 1) & 3)) & 3;
        uint32_t base = pc + 1 + padding;

        if (base + 8 > code_length) {
            return 0;
        }

        if (op == 0xaa) {
            int32_t low;
            int32_t high;
            if (base + 12 > code_length) {
                return 0;
            }
            low = read_s4(code, base + 4);
            high = read_s4(code, base + 8);
            if (high < low) {
                return 0;
            }
            length = 1 + padding + 12 + (uint32_t)(high - low + 1) * 4;
        } else {
            int32_t pairs = read_s4(code, base + 4);
            if (pairs < 0) {
                return 0;
            }
            length = 1 + padding + 8 + (uint32_t)pairs * 8;
        }
    } else if (op >= 0xac && op <= 0xb1) {
        length = 1;
    } else if (op >= 0xb2 && op <= 0xb8) {
        length = 3;
    } else if (op == 0xb9 || op == 0xba) {
        length = 5;
    } else if (op == 0xbb) {
        length = 3;
    } else if (op == 0xbc) {
        length = 2;
    } else if (op == 0xbd) {
        length = 3;
    } else if (op == 0xbe || op == 0xbf) {
        length = 1;
    } else if (op == 0xc0 || op == 0xc1) {
        length = 3;
    } else if (op == 0xc2 || op == 0xc3) {
        length = 1;
    } else if (op == 0xc4) {
        if (pc + 1 >= code_length) {
            return 0;
        }
        length = (code[pc + 1] == 0x84) ? 6 : 4;
    } else if (op == 0xc5) {
        length = 4;
    } else if (op == 0xc6 || op == 0xc7) {
        length = 3;
    } else if (op == 0xc8 || op == 0xc9) {
        length = 5;
    } else if (op == 0xca || op == 0xfe || op == 0xff) {
        length = 1;
    } else {
        return 0;
    }

    if (length == 0 || pc + length > code_length) {
        return 0;
    }

    *out_length = length;
    return 1;
}

static int ldc_supported(const ClassFile *cf, uint16_t index, char *error, size_t error_size) {
    if (cf == NULL || index == 0 || index >= cf->cp_count) {
        snprintf(error, error_size, "invalid constant pool index %u", index);
        return 0;
    }
    if (cf->cp[index].tag != 3 && cf->cp[index].tag != 4) {
        snprintf(error, error_size, "unsupported ldc constant tag %u", cf->cp[index].tag);
        return 0;
    }
    return 1;
}

static int field_supported(const ClassFile *cf, uint16_t index, char *error, size_t error_size) {
    const char *class_name;
    const char *name;
    const char *descriptor;
    const Field *field;

    if (!cp_resolve_fieldref(cf, index, &class_name, &name, &descriptor)) {
        snprintf(error, error_size, "invalid field reference %u", index);
        return 0;
    }

    if (strcmp(class_name, cf->class_name) != 0) {
        snprintf(error, error_size, "external field %s.%s is not supported", class_name, name);
        return 0;
    }

    field = classfile_find_field(cf, name, descriptor);
    if (field == NULL) {
        snprintf(error, error_size, "field %s not found in %s", name, cf->class_name);
        return 0;
    }

    return 1;
}

static int invokespecial_supported(const ClassFile *cf, uint16_t index,
                                   char *error, size_t error_size) {
    const char *class_name;
    const char *name;
    const char *descriptor;

    if (!cp_resolve_methodref(cf, index, &class_name, &name, &descriptor)) {
        snprintf(error, error_size, "invalid method reference %u", index);
        return 0;
    }

    if (strcmp(class_name, "java/lang/Object") == 0 && strcmp(name, "<init>") == 0) {
        return 1;
    }

    snprintf(error, error_size, "invokespecial %s.%s%s is not supported",
             class_name, name, descriptor);
    return 0;
}

static int opcode_supported(const ClassFile *cf, const Method *method, uint32_t pc,
                            uint8_t op, char *error, size_t error_size) {
    const uint8_t *code = method->code;

    switch (op) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x30:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x51:
    case 0x57:
    case 0x59:
    case 0x60:
    case 0x62:
    case 0x64:
    case 0x66:
    case 0x68:
    case 0x6a:
    case 0x6c:
    case 0x6e:
    case 0x70:
    case 0x74:
    case 0x76:
    case 0x78:
    case 0x7a:
    case 0x7c:
    case 0x7e:
    case 0x80:
    case 0x82:
    case 0x86:
    case 0x8b:
    case 0xac:
    case 0xae:
    case 0xb0:
    case 0xb1:
    case 0xbe:
        return 1;
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x17:
    case 0x19:
    case 0x36:
    case 0x38:
    case 0x3a:
    case 0x84:
        return 1;
    case 0x12: {
        uint16_t index = code[pc + 1];
        return ldc_supported(cf, index, error, error_size);
    }
    case 0x13: {
        uint16_t index = (uint16_t)((code[pc + 1] << 8) | code[pc + 2]);
        return ldc_supported(cf, index, error, error_size);
    }
    case 0xb4:
    case 0xb5: {
        uint16_t index = (uint16_t)((code[pc + 1] << 8) | code[pc + 2]);
        return field_supported(cf, index, error, error_size);
    }
    case 0xb7: {
        uint16_t index = (uint16_t)((code[pc + 1] << 8) | code[pc + 2]);
        return invokespecial_supported(cf, index, error, error_size);
    }
    case 0xbc:
        if (code[pc + 1] != 6) {
            snprintf(error, error_size, "only newarray float is supported (atype=%u)",
                     code[pc + 1]);
            return 0;
        }
        return 1;
    default:
        snprintf(error, error_size, "unsupported opcode 0x%02x at offset %u", op, pc);
        return 0;
    }
}

int bytecode_check_method(const ClassFile *cf, const Method *method, const MethodDesc *desc,
                          char *error, size_t error_size) {
    uint32_t pc = 0;

    if (error == NULL || error_size == 0) {
        return 0;
    }
    error[0] = '\0';

    if (method == NULL || method->code == NULL || method->code_length == 0) {
        snprintf(error, error_size, "method has no Code attribute");
        return 0;
    }

    if (desc == NULL || !desc->supported) {
        snprintf(error, error_size, "unsupported method descriptor");
        return 0;
    }

    while (pc < method->code_length) {
        uint8_t op = method->code[pc];
        uint32_t length;

        if (!opcode_length(method->code, method->code_length, pc, &length)) {
            snprintf(error, error_size, "invalid opcode length at offset %u", pc);
            return 0;
        }

        if (!opcode_supported(cf, method, pc, op, error, error_size)) {
            return 0;
        }

        pc += length;
    }

    return 1;
}
