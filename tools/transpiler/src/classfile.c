#include "classfile.h"
#include "descriptor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t length;
    size_t pos;
    int ok;
} Reader;

static uint8_t rd_u1(Reader *r) {
    if (r->pos + 1 > r->length) {
        r->ok = 0;
        return 0;
    }
    return r->data[r->pos++];
}

static uint16_t rd_u2(Reader *r) {
    if (r->pos + 2 > r->length) {
        r->ok = 0;
        return 0;
    }
    {
        uint16_t value = (uint16_t)((r->data[r->pos] << 8) | r->data[r->pos + 1]);
        r->pos += 2;
        return value;
    }
}

static uint32_t rd_u4(Reader *r) {
    if (r->pos + 4 > r->length) {
        r->ok = 0;
        return 0;
    }
    {
        uint32_t value = ((uint32_t)r->data[r->pos] << 24)
                       | ((uint32_t)r->data[r->pos + 1] << 16)
                       | ((uint32_t)r->data[r->pos + 2] << 8)
                       | (uint32_t)r->data[r->pos + 3];
        r->pos += 4;
        return value;
    }
}

static uint64_t rd_u8(Reader *r) {
    uint64_t high = rd_u4(r);
    uint64_t low = rd_u4(r);
    return (high << 32) | low;
}

static const uint8_t *rd_bytes(Reader *r, uint32_t count) {
    const uint8_t *ptr;
    if (r->pos + count > r->length) {
        r->ok = 0;
        return NULL;
    }
    ptr = r->data + r->pos;
    r->pos += count;
    return ptr;
}

static int rd_skip(Reader *r, uint32_t count) {
    if (r->pos + count > r->length) {
        r->ok = 0;
        return 0;
    }
    r->pos += count;
    return 1;
}

static char *dup_string(const char *s) {
    size_t length;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    length = strlen(s);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, length + 1);
    return copy;
}

static char *rd_utf8(Reader *r) {
    uint16_t length = rd_u2(r);
    const uint8_t *bytes;
    char *text;

    if (!r->ok) {
        return NULL;
    }

    bytes = rd_bytes(r, length);
    if (bytes == NULL) {
        return NULL;
    }

    text = malloc((size_t)length + 1);
    if (text == NULL) {
        r->ok = 0;
        return NULL;
    }

    for (uint16_t i = 0; i < length; i++) {
        text[i] = (char)bytes[i];
    }
    text[length] = '\0';
    return text;
}

static int read_constant_pool(Reader *r, ClassFile *cf) {
    cf->cp_count = rd_u2(r);
    if (!r->ok || cf->cp_count == 0) {
        return 0;
    }

    cf->cp = calloc(cf->cp_count, sizeof(CpEntry));
    if (cf->cp == NULL) {
        return 0;
    }

    for (uint16_t i = 1; i < cf->cp_count; i++) {
        CpEntry *entry = &cf->cp[i];
        entry->tag = rd_u1(r);
        if (!r->ok) {
            return 0;
        }

        switch (entry->tag) {
        case 1:
            entry->utf8 = rd_utf8(r);
            if (entry->utf8 == NULL) {
                return 0;
            }
            break;
        case 3:
            entry->int_value = (int32_t)rd_u4(r);
            break;
        case 4: {
            uint32_t bits = rd_u4(r);
            memcpy(&entry->float_value, &bits, sizeof(bits));
            break;
        }
        case 5:
            entry->long_value = rd_u8(r);
            i++;
            break;
        case 6: {
            uint64_t bits = rd_u8(r);
            memcpy(&entry->double_value, &bits, sizeof(bits));
            i++;
            break;
        }
        case 7:
        case 8:
        case 16:
        case 19:
        case 20:
            entry->name_index = rd_u2(r);
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 17:
        case 18:
            entry->class_index = rd_u2(r);
            entry->name_type_index = rd_u2(r);
            if (entry->tag == 12) {
                entry->name_index = entry->class_index;
                entry->descriptor_index = entry->name_type_index;
            }
            break;
        case 15:
            entry->index1 = rd_u1(r);
            entry->index2 = rd_u2(r);
            break;
        default:
            fprintf(stderr, "classfile: unsupported constant pool tag %u at index %u\n",
                    entry->tag, i);
            return 0;
        }

        if (!r->ok) {
            return 0;
        }
    }

    return 1;
}

static int skip_attributes(Reader *r) {
    uint16_t count = rd_u2(r);
    if (!r->ok) {
        return 0;
    }

    for (uint16_t i = 0; i < count; i++) {
        uint16_t name_index = rd_u2(r);
        uint32_t length = rd_u4(r);
        (void)name_index;
        if (!r->ok || !rd_skip(r, length)) {
            return 0;
        }
    }

    return 1;
}

static int read_fields(Reader *r, ClassFile *cf) {
    cf->fields_count = rd_u2(r);
    if (!r->ok) {
        return 0;
    }

    if (cf->fields_count == 0) {
        return 1;
    }

    cf->fields = calloc(cf->fields_count, sizeof(Field));
    if (cf->fields == NULL) {
        return 0;
    }

    for (uint16_t i = 0; i < cf->fields_count; i++) {
        Field *field = &cf->fields[i];
        uint16_t name_index;
        uint16_t descriptor_index;

        field->access_flags = rd_u2(r);
        name_index = rd_u2(r);
        descriptor_index = rd_u2(r);
        field->is_static = (field->access_flags & 0x0008) != 0;
        field->name = dup_string(cp_utf8(cf, name_index));
        field->descriptor = dup_string(cp_utf8(cf, descriptor_index));
        if (field->name == NULL || field->descriptor == NULL) {
            return 0;
        }
        if (!skip_attributes(r)) {
            return 0;
        }
    }

    return 1;
}

static int read_methods(Reader *r, ClassFile *cf) {
    cf->methods_count = rd_u2(r);
    if (!r->ok) {
        return 0;
    }

    if (cf->methods_count == 0) {
        return 1;
    }

    cf->methods = calloc(cf->methods_count, sizeof(Method));
    if (cf->methods == NULL) {
        return 0;
    }

    for (uint16_t i = 0; i < cf->methods_count; i++) {
        Method *method = &cf->methods[i];
        uint16_t name_index;
        uint16_t descriptor_index;
        uint16_t attribute_count;

        method->access_flags = rd_u2(r);
        name_index = rd_u2(r);
        descriptor_index = rd_u2(r);
        method->name = dup_string(cp_utf8(cf, name_index));
        method->descriptor = dup_string(cp_utf8(cf, descriptor_index));
        if (method->name == NULL || method->descriptor == NULL) {
            return 0;
        }

        attribute_count = rd_u2(r);
        if (!r->ok) {
            return 0;
        }

        for (uint16_t j = 0; j < attribute_count; j++) {
            uint16_t attr_name_index = rd_u2(r);
            uint32_t attr_length = rd_u4(r);
            const char *attr_name = cp_utf8(cf, attr_name_index);

            if (!r->ok) {
                return 0;
            }

            if (attr_name != NULL && strcmp(attr_name, "Code") == 0) {
                uint32_t code_length;
                uint16_t exception_count;
                const uint8_t *code;

                method->max_stack = rd_u2(r);
                method->max_locals = rd_u2(r);
                code_length = rd_u4(r);
                code = rd_bytes(r, code_length);
                if (!r->ok || code == NULL) {
                    return 0;
                }

                method->code_length = code_length;
                method->code = malloc(code_length == 0 ? 1 : code_length);
                if (method->code == NULL) {
                    return 0;
                }
                if (code_length != 0) {
                    memcpy(method->code, code, code_length);
                }

                exception_count = rd_u2(r);
                if (!r->ok || !rd_skip(r, (uint32_t)exception_count * 8)) {
                    return 0;
                }
                if (!skip_attributes(r)) {
                    return 0;
                }
            } else {
                if (!rd_skip(r, attr_length)) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

static void layout_field_size(const char *descriptor, uint32_t *size, uint32_t *alignment) {
    VType type = VT_UNSUPPORTED;
    if (descriptor_parse_field(descriptor, &type) && type == VT_REF) {
        *size = 8;
        *alignment = 8;
        return;
    }
    if (descriptor_parse_field(descriptor, &type) && (type == VT_LONG || type == VT_DOUBLE)) {
        *size = 8;
        *alignment = 8;
        return;
    }
    *size = 4;
    *alignment = 4;
}

void classfile_layout_fields(ClassFile *cf) {
    uint32_t offset = 8;

    for (uint16_t i = 0; i < cf->fields_count; i++) {
        Field *field = &cf->fields[i];
        uint32_t size;
        uint32_t alignment;

        if (field->is_static) {
            field->offset = 0;
            field->size = 0;
            continue;
        }

        layout_field_size(field->descriptor, &size, &alignment);
        offset = (offset + alignment - 1) & ~(alignment - 1);
        field->offset = offset;
        field->size = size;
        offset += size;
    }

    cf->object_size = (offset + 15) & ~15U;
    if (cf->object_size < 16) {
        cf->object_size = 16;
    }
}

ClassFile *classfile_load(const char *path) {
    FILE *file;
    long file_size;
    uint8_t *data;
    Reader reader;
    ClassFile *cf;

    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = malloc((size_t)file_size);
    if (data == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);

    reader.data = data;
    reader.length = (size_t)file_size;
    reader.pos = 0;
    reader.ok = 1;

    if (rd_u4(&reader) != 0xCAFEBABE) {
        fprintf(stderr, "classfile: bad magic in %s\n", path);
        free(data);
        return NULL;
    }

    cf = calloc(1, sizeof(ClassFile));
    if (cf == NULL) {
        free(data);
        return NULL;
    }

    (void)rd_u2(&reader);
    cf->major_version = rd_u2(&reader);
    if (!read_constant_pool(&reader, cf)) {
        fprintf(stderr, "classfile: failed to read constant pool\n");
        goto fail;
    }

    cf->access_flags = rd_u2(&reader);
    cf->this_class = rd_u2(&reader);
    cf->super_class = rd_u2(&reader);
    if (!reader.ok) {
        goto fail;
    }

    {
        uint16_t interfaces_count = rd_u2(&reader);
        if (!reader.ok || !rd_skip(&reader, (uint32_t)interfaces_count * 2)) {
            goto fail;
        }
    }

    if (!read_fields(&reader, cf)) {
        goto fail;
    }
    if (!read_methods(&reader, cf)) {
        goto fail;
    }
    if (!skip_attributes(&reader)) {
        goto fail;
    }

    cf->class_name = dup_string(cp_class_name(cf, cf->this_class));
    if (cf->class_name == NULL) {
        goto fail;
    }

    classfile_layout_fields(cf);
    free(data);
    return cf;

fail:
    free(data);
    classfile_free(cf);
    return NULL;
}

void classfile_free(ClassFile *cf) {
    if (cf == NULL) {
        return;
    }

    if (cf->cp != NULL) {
        for (uint16_t i = 0; i < cf->cp_count; i++) {
            free(cf->cp[i].utf8);
        }
        free(cf->cp);
    }

    if (cf->fields != NULL) {
        for (uint16_t i = 0; i < cf->fields_count; i++) {
            free(cf->fields[i].name);
            free(cf->fields[i].descriptor);
        }
        free(cf->fields);
    }

    if (cf->methods != NULL) {
        for (uint16_t i = 0; i < cf->methods_count; i++) {
            free(cf->methods[i].name);
            free(cf->methods[i].descriptor);
            free(cf->methods[i].code);
        }
        free(cf->methods);
    }

    free(cf->class_name);
    free(cf);
}

const char *cp_utf8(const ClassFile *cf, uint16_t index) {
    if (cf == NULL || cf->cp == NULL || index == 0 || index >= cf->cp_count) {
        return NULL;
    }
    if (cf->cp[index].tag != 1) {
        return NULL;
    }
    return cf->cp[index].utf8;
}

const char *cp_class_name(const ClassFile *cf, uint16_t index) {
    if (cf == NULL || cf->cp == NULL || index == 0 || index >= cf->cp_count) {
        return NULL;
    }
    if (cf->cp[index].tag != 7) {
        return NULL;
    }
    return cp_utf8(cf, cf->cp[index].name_index);
}

int cp_name_and_type(const ClassFile *cf, uint16_t index, const char **name, const char **descriptor) {
    if (cf == NULL || cf->cp == NULL || index == 0 || index >= cf->cp_count) {
        return 0;
    }
    if (cf->cp[index].tag != 12) {
        return 0;
    }
    *name = cp_utf8(cf, cf->cp[index].name_index);
    *descriptor = cp_utf8(cf, cf->cp[index].descriptor_index);
    return *name != NULL && *descriptor != NULL;
}

int cp_resolve_fieldref(const ClassFile *cf, uint16_t index,
                        const char **class_name, const char **name, const char **descriptor) {
    const CpEntry *entry;

    if (cf == NULL || cf->cp == NULL || index == 0 || index >= cf->cp_count) {
        return 0;
    }

    entry = &cf->cp[index];
    if (entry->tag != 9) {
        return 0;
    }

    *class_name = cp_class_name(cf, entry->class_index);
    if (*class_name == NULL) {
        return 0;
    }
    return cp_name_and_type(cf, entry->name_type_index, name, descriptor);
}

int cp_resolve_methodref(const ClassFile *cf, uint16_t index,
                         const char **class_name, const char **name, const char **descriptor) {
    const CpEntry *entry;

    if (cf == NULL || cf->cp == NULL || index == 0 || index >= cf->cp_count) {
        return 0;
    }

    entry = &cf->cp[index];
    if (entry->tag != 10 && entry->tag != 11) {
        return 0;
    }

    *class_name = cp_class_name(cf, entry->class_index);
    if (*class_name == NULL) {
        return 0;
    }
    return cp_name_and_type(cf, entry->name_type_index, name, descriptor);
}

const Field *classfile_find_field(const ClassFile *cf, const char *name, const char *descriptor) {
    if (cf == NULL || name == NULL || descriptor == NULL) {
        return NULL;
    }
    for (uint16_t i = 0; i < cf->fields_count; i++) {
        const Field *field = &cf->fields[i];
        if (!field->is_static
            && strcmp(field->name, name) == 0
            && strcmp(field->descriptor, descriptor) == 0) {
            return field;
        }
    }
    return NULL;
}
