#pragma once
#include "lpg_unicode_string.h"
#include "lpg_struct_member_id.h"
#include "lpg_enum_element_id.h"

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

typedef struct enumeration_element
{
    unicode_string name;
} enumeration_element;

enumeration_element enumeration_element_create(unicode_string name);
void enumeration_element_free(enumeration_element const *value);

typedef struct enumeration
{
    enumeration_element *elements;
    enum_element_id size;
} enumeration;

enumeration enumeration_create(enumeration_element *elements,
                               enum_element_id size);
void enumeration_free(enumeration const *value);

typedef enum type_kind
{
    type_kind_structure,
    type_kind_function_pointer,
    type_kind_unit,
    type_kind_string_ref,
    type_kind_enumeration,
    type_kind_referenced,
    type_kind_type
} type_kind;

struct type
{
    type_kind kind;
    union
    {
        structure structure_;
        function_pointer function_pointer_;
        enumeration enum_;
        type const *referenced;
    };
};

void type_free(type const *value);
void type_deallocate(type *value);
type type_from_function_pointer(function_pointer value);
type type_from_unit(void);
type type_from_string_ref(void);
type type_from_enumeration(enumeration const value);
type type_from_reference(type const *const referenced);
type type_from_type(void);
type *type_allocate(type const value);

function_pointer function_pointer_create(type *result, type *arguments,
                                         size_t arity);
void function_pointer_free(function_pointer const *value);
