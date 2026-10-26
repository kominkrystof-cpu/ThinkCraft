#include "emit_nasm.h"
#include "bytecode.h"
#include "descriptor.h"
#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_STACK_VALUES 1024

static const char *int_regs32[6] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };

static int align16(int value) {
    return (value + 15) & ~15;
}

static int local_offset(int index) {
    return 8 * (index + 1);
}

static int temp_offset(int max_locals, int depth) {
    return 8 * (max_locals + depth + 1);
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void append_arg_codes(char *buffer, size_t size, const MethodDesc *desc) {
    size_t length = strlen(buffer);

    for (int i = 0; i < desc->nargs; i++) {
        const char *code = NULL;
        switch (desc->args[i]) {
        case VT_INT: code = "i"; break;
        case VT_FLOAT: code = "f"; break;
        case VT_REF: code = "ref"; break;
        case VT_LONG: code = "l"; break;
        case VT_DOUBLE: code = "d"; break;
        default: code = "x"; break;
        }
        snprintf(buffer + length, size - length, "_%s", code);
        length = strlen(buffer);
    }
}

static void build_method_symbol(char *buffer, size_t size, const char *prefix,
                                const char *name, const MethodDesc *desc) {
    if (strcmp(name, "<init>") == 0) {
        snprintf(buffer, size, "%s_init", prefix);
        return;
    }

    snprintf(buffer, size, "%s_%s", prefix, name);
    append_arg_codes(buffer, size, desc);
}

static void build_constructor_symbol(char *buffer, size_t size, const char *prefix) {
    snprintf(buffer, size, "%s_new", prefix);
}

static void emit_prologue(FILE *out, int frame_size) {
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");
    fprintf(out, "    sub rsp, %d\n", frame_size);
}

static void emit_epilogue(FILE *out) {
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
}

static int emit_store_args(FILE *out, const Method *method, const MethodDesc *desc) {
    int int_reg = 0;
    int float_reg = 0;
    int local = 0;

    if ((method->access_flags & 0x0008) == 0) {
        fprintf(out, "    mov qword [rbp - %d], rdi\n", local_offset(local));
        local++;
        int_reg = 1;
    }

    for (int i = 0; i < desc->nargs; i++) {
        if (desc->args[i] == VT_INT) {
            if (int_reg >= 6) {
                fprintf(stderr, "emit: too many integer arguments in %s\n", method->name);
                return 0;
            }
            fprintf(out, "    mov dword [rbp - %d], %s\n",
                    local_offset(local), int_regs32[int_reg]);
            int_reg++;
        } else if (desc->args[i] == VT_FLOAT) {
            if (float_reg >= 8) {
                fprintf(stderr, "emit: too many float arguments in %s\n", method->name);
                return 0;
            }
            fprintf(out, "    movss [rbp - %d], xmm%d\n",
                    local_offset(local), float_reg);
            float_reg++;
        } else {
            fprintf(stderr, "emit: unsupported argument type in %s\n", method->name);
            return 0;
        }
        local++;
    }

    return 1;
}

static int emit_method(FILE *out, ClassFile *cf, Method *method, const MethodDesc *desc,
                       const char *symbol, int make_global, int verbose) {
    const uint8_t *code = method->code;
    uint32_t code_length = method->code_length;
    uint32_t pc = 0;
    int max_locals = method->max_locals;
    int max_stack = method->max_stack;
    int frame_size = align16(16 + 8 * (max_locals + max_stack));
    int depth = 0;
    int did_return = 0;
    VType stack[MAX_STACK_VALUES];

    if (make_global) {
        fprintf(out, "global %s\n", symbol);
    }
    fprintf(out, "; %s%s%s\n", method->name, method->descriptor,
            verbose ? "" : "");
    fprintf(out, "%s:\n", symbol);
    emit_prologue(out, frame_size);

    if (!emit_store_args(out, method, desc)) {
        return 0;
    }

    while (pc < code_length) {
        uint8_t op = code[pc];
        uint32_t length = 0;

        if (!opcode_length(code, code_length, pc, &length)) {
            fprintf(stderr, "emit: invalid opcode length in %s at %u\n", method->name, pc);
            return 0;
        }

        if (depth < 0 || depth > MAX_STACK_VALUES) {
            fprintf(stderr, "emit: stack depth out of range in %s\n", method->name);
            return 0;
        }

        switch (op) {
        case 0x00:
            break;

        case 0x01: {
            int dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov qword [rbp - %d], 0\n", dst);
            stack[depth++] = VT_REF;
            break;
        }

        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08: {
            int value = (op == 0x02) ? -1 : (op - 0x03);
            int dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], %d\n", dst, value);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x0b:
        case 0x0c:
        case 0x0d: {
            uint32_t bits = float_bits((float)(op - 0x0b));
            int dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], 0x%08x\n", dst, bits);
            stack[depth++] = VT_FLOAT;
            break;
        }

        case 0x10: {
            int8_t value = (int8_t)code[pc + 1];
            int dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], %d\n", dst, value);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x11: {
            int16_t value = (int16_t)(((uint16_t)code[pc + 1] << 8) | code[pc + 2]);
            int dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], %d\n", dst, value);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x12:
        case 0x13: {
            uint16_t index = (op == 0x12)
                ? (uint16_t)code[pc + 1]
                : (uint16_t)(((uint16_t)code[pc + 1] << 8) | code[pc + 2]);
            CpEntry *entry = &cf->cp[index];
            int dst = temp_offset(max_locals, depth);

            if (entry->tag == 3) {
                fprintf(out, "    mov dword [rbp - %d], %d\n", dst, entry->int_value);
                stack[depth++] = VT_INT;
            } else if (entry->tag == 4) {
                fprintf(out, "    mov dword [rbp - %d], 0x%08x\n",
                        dst, float_bits(entry->float_value));
                stack[depth++] = VT_FLOAT;
            } else {
                fprintf(stderr, "emit: unsupported ldc tag %u\n", entry->tag);
                return 0;
            }
            break;
        }

        case 0x15:
        case 0x17:
        case 0x19: {
            int index = code[pc + 1];
            int src = local_offset(index);
            int dst = temp_offset(max_locals, depth);
            if (op == 0x15) {
                fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
                fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
                stack[depth++] = VT_INT;
            } else if (op == 0x17) {
                fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
                fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
                stack[depth++] = VT_FLOAT;
            } else {
                fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
                fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
                stack[depth++] = VT_REF;
            }
            break;
        }

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
        case 0x2d: {
            int index;
            int is_int = op >= 0x1a && op <= 0x1d;
            int is_float = op >= 0x22 && op <= 0x25;
            int src;
            int dst;

            if (is_int) {
                index = op - 0x1a;
            } else if (is_float) {
                index = op - 0x22;
            } else {
                index = op - 0x2a;
            }

            src = local_offset(index);
            dst = temp_offset(max_locals, depth);
            if (is_int) {
                fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
                fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
                stack[depth++] = VT_INT;
            } else if (is_float) {
                fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
                fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
                stack[depth++] = VT_FLOAT;
            } else {
                fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
                fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
                stack[depth++] = VT_REF;
            }
            break;
        }

        case 0x36:
        case 0x38:
        case 0x3a: {
            int index = code[pc + 1];
            int dst = local_offset(index);
            int src = temp_offset(max_locals, depth - 1);
            depth--;
            if (op == 0x36) {
                fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
                fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            } else if (op == 0x38) {
                fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
                fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
            } else {
                fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
                fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
            }
            break;
        }

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
        case 0x4e: {
            int is_int = op >= 0x3b && op <= 0x3e;
            int is_float = op >= 0x43 && op <= 0x46;
            int index;
            int dst;
            int src;

            if (is_int) {
                index = op - 0x3b;
            } else if (is_float) {
                index = op - 0x43;
            } else {
                index = op - 0x4b;
            }

            dst = local_offset(index);
            src = temp_offset(max_locals, depth - 1);
            depth--;
            if (is_int) {
                fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
                fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            } else if (is_float) {
                fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
                fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
            } else {
                fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
                fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
            }
            break;
        }

        case 0x30: {
            int index_off = temp_offset(max_locals, depth - 1);
            int array_off = temp_offset(max_locals, depth - 2);
            int dst;
            depth -= 2;
            fprintf(out, "    mov rax, qword [rbp - %d]\n", array_off);
            fprintf(out, "    mov ecx, dword [rbp - %d]\n", index_off);
            fprintf(out, "    movss xmm0, [rax + rcx*4 + 8]\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
            stack[depth++] = VT_FLOAT;
            break;
        }

        case 0x51: {
            int value_off = temp_offset(max_locals, depth - 1);
            int index_off = temp_offset(max_locals, depth - 2);
            int array_off = temp_offset(max_locals, depth - 3);
            depth -= 3;
            fprintf(out, "    mov rax, qword [rbp - %d]\n", array_off);
            fprintf(out, "    mov ecx, dword [rbp - %d]\n", index_off);
            fprintf(out, "    movss xmm0, [rbp - %d]\n", value_off);
            fprintf(out, "    movss [rax + rcx*4 + 8], xmm0\n");
            break;
        }

        case 0x57:
            if (depth <= 0) {
                fprintf(stderr, "emit: pop on empty stack in %s\n", method->name);
                return 0;
            }
            depth--;
            break;

        case 0x59: {
            int src;
            int dst;
            VType type;
            if (depth <= 0) {
                fprintf(stderr, "emit: dup on empty stack in %s\n", method->name);
                return 0;
            }
            type = stack[depth - 1];
            src = temp_offset(max_locals, depth - 1);
            dst = temp_offset(max_locals, depth);
            if (type == VT_REF) {
                fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
                fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
            } else {
                fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
                fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            }
            stack[depth] = type;
            depth++;
            break;
        }

        case 0x60:
        case 0x64:
        case 0x68:
        case 0x6c:
        case 0x70:
        case 0x78:
        case 0x7a:
        case 0x7c:
        case 0x7e:
        case 0x80:
        case 0x82: {
            int b_off = temp_offset(max_locals, depth - 1);
            int a_off = temp_offset(max_locals, depth - 2);
            int dst;
            depth -= 2;
            fprintf(out, "    mov eax, dword [rbp - %d]\n", a_off);
            fprintf(out, "    mov ecx, dword [rbp - %d]\n", b_off);
            switch (op) {
            case 0x60: fprintf(out, "    add eax, ecx\n"); break;
            case 0x64: fprintf(out, "    sub eax, ecx\n"); break;
            case 0x68: fprintf(out, "    imul eax, ecx\n"); break;
            case 0x6c:
            case 0x70:
                fprintf(out, "    cdq\n");
                fprintf(out, "    idiv ecx\n");
                if (op == 0x70) {
                    fprintf(out, "    mov eax, edx\n");
                }
                break;
            case 0x78: fprintf(out, "    shl eax, cl\n"); break;
            case 0x7a: fprintf(out, "    sar eax, cl\n"); break;
            case 0x7c: fprintf(out, "    shr eax, cl\n"); break;
            case 0x7e: fprintf(out, "    and eax, ecx\n"); break;
            case 0x80: fprintf(out, "    or eax, ecx\n"); break;
            case 0x82: fprintf(out, "    xor eax, ecx\n"); break;
            }
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x62:
        case 0x66:
        case 0x6a:
        case 0x6e: {
            int b_off = temp_offset(max_locals, depth - 1);
            int a_off = temp_offset(max_locals, depth - 2);
            int dst;
            depth -= 2;
            fprintf(out, "    movss xmm0, [rbp - %d]\n", a_off);
            fprintf(out, "    movss xmm1, [rbp - %d]\n", b_off);
            switch (op) {
            case 0x62: fprintf(out, "    addss xmm0, xmm1\n"); break;
            case 0x66: fprintf(out, "    subss xmm0, xmm1\n"); break;
            case 0x6a: fprintf(out, "    mulss xmm0, xmm1\n"); break;
            case 0x6e: fprintf(out, "    divss xmm0, xmm1\n"); break;
            }
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
            stack[depth++] = VT_FLOAT;
            break;
        }

        case 0x74: {
            int src = temp_offset(max_locals, depth - 1);
            int dst;
            depth--;
            fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
            fprintf(out, "    neg eax\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x76: {
            int src = temp_offset(max_locals, depth - 1);
            int dst;
            depth--;
            fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
            fprintf(out, "    xor eax, 0x80000000\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            stack[depth++] = VT_FLOAT;
            break;
        }

        case 0x86: {
            int src = temp_offset(max_locals, depth - 1);
            int dst;
            depth--;
            fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
            fprintf(out, "    cvtsi2ss xmm0, eax\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
            stack[depth++] = VT_FLOAT;
            break;
        }

        case 0x8b: {
            int src = temp_offset(max_locals, depth - 1);
            int dst;
            depth--;
            fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
            fprintf(out, "    cvttss2si eax, xmm0\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            stack[depth++] = VT_INT;
            break;
        }

        case 0x84: {
            int index = code[pc + 1];
            int8_t delta = (int8_t)code[pc + 2];
            int dst = local_offset(index);
            fprintf(out, "    mov eax, dword [rbp - %d]\n", dst);
            fprintf(out, "    add eax, %d\n", delta);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            break;
        }

        case 0xb4:
        case 0xb5: {
            uint16_t index = (uint16_t)(((uint16_t)code[pc + 1] << 8) | code[pc + 2]);
            const char *class_name;
            const char *name;
            const char *descriptor;
            const Field *field;
            VType field_type = VT_UNSUPPORTED;

            if (!cp_resolve_fieldref(cf, index, &class_name, &name, &descriptor)) {
                fprintf(stderr, "emit: invalid field ref at %u\n", pc);
                return 0;
            }
            field = classfile_find_field(cf, name, descriptor);
            if (field == NULL || !descriptor_parse_field(descriptor, &field_type)) {
                fprintf(stderr, "emit: field %s%s not found\n", name, descriptor);
                return 0;
            }

            if (op == 0xb4) {
                int ref_off = temp_offset(max_locals, depth - 1);
                int dst;
                depth--;
                fprintf(out, "    mov rax, qword [rbp - %d]\n", ref_off);
                dst = temp_offset(max_locals, depth);
                if (field_type == VT_INT) {
                    fprintf(out, "    mov eax, dword [rax + %u]\n", field->offset);
                    fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
                } else if (field_type == VT_FLOAT) {
                    fprintf(out, "    movss xmm0, [rax + %u]\n", field->offset);
                    fprintf(out, "    movss [rbp - %d], xmm0\n", dst);
                } else if (field_type == VT_REF) {
                    fprintf(out, "    mov rax, qword [rax + %u]\n", field->offset);
                    fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
                } else {
                    fprintf(stderr, "emit: unsupported field type %s\n", descriptor);
                    return 0;
                }
                stack[depth++] = field_type;
            } else {
                int value_off = temp_offset(max_locals, depth - 1);
                int ref_off = temp_offset(max_locals, depth - 2);
                VType value_type = stack[depth - 1];
                depth -= 2;

                if (value_type == VT_INT) {
                    fprintf(out, "    mov eax, dword [rbp - %d]\n", value_off);
                    fprintf(out, "    mov rdx, qword [rbp - %d]\n", ref_off);
                    fprintf(out, "    mov dword [rdx + %u], eax\n", field->offset);
                } else if (value_type == VT_FLOAT) {
                    fprintf(out, "    movss xmm0, [rbp - %d]\n", value_off);
                    fprintf(out, "    mov rdx, qword [rbp - %d]\n", ref_off);
                    fprintf(out, "    movss [rdx + %u], xmm0\n", field->offset);
                } else if (value_type == VT_REF) {
                    fprintf(out, "    mov rax, qword [rbp - %d]\n", value_off);
                    fprintf(out, "    mov rdx, qword [rbp - %d]\n", ref_off);
                    fprintf(out, "    mov qword [rdx + %u], rax\n", field->offset);
                } else {
                    fprintf(stderr, "emit: unsupported putfield value type\n");
                    return 0;
                }
            }
            break;
        }

        case 0xb7: {
            uint16_t index = (uint16_t)(((uint16_t)code[pc + 1] << 8) | code[pc + 2]);
            const char *class_name;
            const char *name;
            const char *descriptor;

            if (!cp_resolve_methodref(cf, index, &class_name, &name, &descriptor)) {
                fprintf(stderr, "emit: invalid method ref at %u\n", pc);
                return 0;
            }
            if (strcmp(class_name, "java/lang/Object") != 0 || strcmp(name, "<init>") != 0) {
                fprintf(stderr, "emit: unsupported invokespecial %s.%s%s\n",
                        class_name, name, descriptor);
                return 0;
            }
            if (depth <= 0) {
                fprintf(stderr, "emit: invokespecial on empty stack\n");
                return 0;
            }
            depth--;
            break;
        }

        case 0xbc: {
            int count_off;
            int dst;
            if (code[pc + 1] != 6) {
                fprintf(stderr, "emit: unsupported newarray atype %u\n", code[pc + 1]);
                return 0;
            }
            count_off = temp_offset(max_locals, depth - 1);
            depth--;
            fprintf(out, "    mov edi, dword [rbp - %d]\n", count_off);
            fprintf(out, "    call rt_new_float_array\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov qword [rbp - %d], rax\n", dst);
            stack[depth++] = VT_REF;
            break;
        }

        case 0xbe: {
            int ref_off = temp_offset(max_locals, depth - 1);
            int dst;
            depth--;
            fprintf(out, "    mov rax, qword [rbp - %d]\n", ref_off);
            fprintf(out, "    mov eax, dword [rax]\n");
            dst = temp_offset(max_locals, depth);
            fprintf(out, "    mov dword [rbp - %d], eax\n", dst);
            stack[depth++] = VT_INT;
            break;
        }

        case 0xac: {
            int src = temp_offset(max_locals, depth - 1);
            fprintf(out, "    mov eax, dword [rbp - %d]\n", src);
            emit_epilogue(out);
            did_return = 1;
            pc = code_length;
            break;
        }

        case 0xae: {
            int src = temp_offset(max_locals, depth - 1);
            fprintf(out, "    movss xmm0, [rbp - %d]\n", src);
            emit_epilogue(out);
            did_return = 1;
            pc = code_length;
            break;
        }

        case 0xb0: {
            int src = temp_offset(max_locals, depth - 1);
            fprintf(out, "    mov rax, qword [rbp - %d]\n", src);
            emit_epilogue(out);
            did_return = 1;
            pc = code_length;
            break;
        }

        case 0xb1:
            emit_epilogue(out);
            did_return = 1;
            pc = code_length;
            break;

        default:
            fprintf(stderr, "emit: unsupported opcode 0x%02x in %s at %u\n",
                    op, method->name, pc);
            return 0;
        }

        if (pc == code_length) {
            break;
        }
        pc += length;
    }

    if (!did_return) {
        emit_epilogue(out);
    }

    return 1;
}

static int emit_constructor_wrapper(FILE *out, ClassFile *cf, const Method *method,
                                    const MethodDesc *desc, const char *init_symbol,
                                    const char *prefix) {
    char wrapper_symbol[256];
    int frame_size = align16(16 + 8 * (desc->nargs + 1));
    int object_offset = 8 * (desc->nargs + 1);
    int int_reg = 0;
    int float_reg = 0;

    build_constructor_symbol(wrapper_symbol, sizeof(wrapper_symbol), prefix);

    fprintf(out, "global %s\n", wrapper_symbol);
    fprintf(out, "; generated constructor wrapper for %s\n", method->descriptor);
    fprintf(out, "%s:\n", wrapper_symbol);
    emit_prologue(out, frame_size);

    for (int i = 0; i < desc->nargs; i++) {
        if (desc->args[i] == VT_INT) {
            if (int_reg >= 6) {
                return 0;
            }
            fprintf(out, "    mov dword [rbp - %d], %s\n",
                    local_offset(i), int_regs32[int_reg++]);
        } else if (desc->args[i] == VT_FLOAT) {
            if (float_reg >= 8) {
                return 0;
            }
            fprintf(out, "    movss [rbp - %d], xmm%d\n",
                    local_offset(i), float_reg++);
        } else {
            return 0;
        }
    }

    fprintf(out, "    mov edi, %u\n", cf->object_size);
    fprintf(out, "    call rt_alloc\n");
    fprintf(out, "    mov qword [rbp - %d], rax\n", object_offset);
    fprintf(out, "    mov rdi, rax\n");

    int_reg = 1;
    float_reg = 0;
    for (int i = 0; i < desc->nargs; i++) {
        if (desc->args[i] == VT_INT) {
            if (int_reg >= 6) {
                return 0;
            }
            fprintf(out, "    mov %s, dword [rbp - %d]\n",
                    int_regs32[int_reg++], local_offset(i));
        } else if (desc->args[i] == VT_FLOAT) {
            if (float_reg >= 8) {
                return 0;
            }
            fprintf(out, "    movss xmm%d, [rbp - %d]\n",
                    float_reg++, local_offset(i));
        }
    }

    fprintf(out, "    call %s\n", init_symbol);
    fprintf(out, "    mov rax, qword [rbp - %d]\n", object_offset);
    emit_epilogue(out);
    return 1;
}

int emit_nasm(ClassFile *cf, const char *out_path, const char *prefix, int verbose) {
    FILE *out;
    int constructor_emitted = 0;

    out = fopen(out_path, "w");
    if (out == NULL) {
        perror(out_path);
        return 0;
    }

    fprintf(out, "; generated by mc-transpiler\n");
    fprintf(out, "; source class: %s\n", cf->class_name);
    fprintf(out, "default rel\n\n");
    fprintf(out, "section .text\n\n");
    fprintf(out, "extern rt_alloc\n");
    fprintf(out, "extern rt_new_float_array\n\n");

    for (uint16_t i = 0; i < cf->methods_count; i++) {
        Method *method = &cf->methods[i];
        MethodDesc desc;
        char error[256];
        char symbol[256];

        if (!descriptor_parse_method(method->descriptor, &desc)) {
            if (verbose) {
                fprintf(stderr, "skip %s%s: unsupported descriptor\n",
                        method->name, method->descriptor);
            }
            continue;
        }

        if (!bytecode_check_method(cf, method, &desc, error, sizeof(error))) {
            if (verbose) {
                fprintf(stderr, "skip %s%s: %s\n",
                        method->name, method->descriptor, error);
            }
            continue;
        }

        if (strcmp(method->name, "<init>") == 0) {
            if (constructor_emitted) {
                if (verbose) {
                    fprintf(stderr, "skip additional constructor %s\n", method->descriptor);
                }
                continue;
            }

            build_method_symbol(symbol, sizeof(symbol), prefix, method->name, &desc);
            if (!emit_method(out, cf, method, &desc, symbol, 0, verbose)) {
                fclose(out);
                return 0;
            }
            if (!emit_constructor_wrapper(out, cf, method, &desc, symbol, prefix)) {
                fclose(out);
                return 0;
            }
            constructor_emitted = 1;
        } else {
            build_method_symbol(symbol, sizeof(symbol), prefix, method->name, &desc);
            if (!emit_method(out, cf, method, &desc, symbol, 1, verbose)) {
                fclose(out);
                return 0;
            }
        }
    }

    fprintf(out, "\nsection .note.GNU-stack noalloc noexec nowrite progbits\n");

    if (ferror(out)) {
        fclose(out);
        return 0;
    }
    fclose(out);
    return 1;
}
