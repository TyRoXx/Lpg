#pragma once
#include "lpg_unicode_string.h"

typedef uint32_t struct_member_id;

typedef struct structure_member structure_member;

typedef struct structure
{
    structure_member *members;
    struct_member_id count;
} structure;

structure structure_create(structure_member *members, struct_member_id count);
void structure_free(structure const *value);

typedef struct type type;

typedef struct function_pointer
{
    type *result;
    type *arguments;
    size_t arity;
} function_pointer;

typedef enum type_kind
{
    type_kind_structure,
    type_kind_function_pointer,
    type_kind_unit,
    type_kind_string_ref
} type_kind;

struct type
{
    type_kind kind;
    union
    {
        structure structure_;
        function_pointer function_pointer_;
    };
};

void type_free(type const *value);
void type_deallocate(type *value);
type type_from_function_pointer(function_pointer value);
type type_from_unit(void);
type type_from_string_ref(void);
type *type_allocate(type const value);

function_pointer function_pointer_create(type *result, type *arguments,
                                         size_t arity);
void function_pointer_free(function_pointer const *value);

struct structure_member
{
    type what;
    unicode_string name;
};

structure_member structure_member_create(type what, unicode_string name);
void struct_member_free(structure_member const *value);
