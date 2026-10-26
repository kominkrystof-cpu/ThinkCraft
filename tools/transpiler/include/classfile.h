#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <stdint.h>

typedef struct {
    uint8_t tag;
    char *utf8;
    uint16_t class_index;
    uint16_t name_type_index;
    uint16_t name_index;
    uint16_t descriptor_index;
    uint16_t index1;
    uint16_t index2;
    int32_t int_value;
    float float_value;
    uint64_t long_value;
    double double_value;
} CpEntry;

typedef struct {
    uint16_t access_flags;
    char *name;
    char *descriptor;
    uint32_t offset;
    uint32_t size;
    int is_static;
} Field;

typedef struct {
    uint16_t access_flags;
    char *name;
    char *descriptor;
    uint16_t max_stack;
    uint16_t max_locals;
    uint32_t code_length;
    uint8_t *code;
} Method;

typedef struct {
    uint16_t major_version;
    uint16_t access_flags;
    uint16_t this_class;
    uint16_t super_class;
    CpEntry *cp;
    uint16_t cp_count;
    Field *fields;
    uint16_t fields_count;
    Method *methods;
    uint16_t methods_count;
    uint32_t object_size;
    char *class_name;
} ClassFile;

ClassFile *classfile_load(const char *path);
void classfile_free(ClassFile *cf);

const char *cp_utf8(const ClassFile *cf, uint16_t index);
const char *cp_class_name(const ClassFile *cf, uint16_t index);
int cp_name_and_type(const ClassFile *cf, uint16_t index, const char **name, const char **descriptor);
int cp_resolve_fieldref(const ClassFile *cf, uint16_t index, const char **class_name, const char **name, const char **descriptor);
int cp_resolve_methodref(const ClassFile *cf, uint16_t index, const char **class_name, const char **name, const char **descriptor);

const Field *classfile_find_field(const ClassFile *cf, const char *name, const char *descriptor);
void classfile_layout_fields(ClassFile *cf);

#endif
