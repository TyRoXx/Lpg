#pragma once

#include "lpg_unicode_string.h"
#include "lpg_struct_member_id.h"
#include "lpg_enum_element_id.h"
#include "lpg_integer.h"

typedef struct structure_member structure_member;

typedef struct structure
{
    structure_member *members;
    struct_member_id count;
} structure;

structure structure_create(structure_member *members, struct_member_id count);
void structure_free(structure const *value);

typedef struct type type;

typedef struct enumeration_element
{
    unicode_string name;
} enumeration_element;

enumeration_element enumeration_element_create(unicode_string name);
void enumeration_element_free(LPG_NON_NULL(enumeration_element const *value));

typedef struct enumeration
{
    enumeration_element *elements;
    enum_element_id size;
} enumeration;

enumeration enumeration_create(enumeration_element *elements,
                               enum_element_id size);
void enumeration_free(LPG_NON_NULL(enumeration const *value));

typedef struct tuple_type
{
    type *elements;
    size_t length;
} tuple_type;

bool tuple_type_equals(tuple_type const left, tuple_type const right);
typedef struct integer_range
{
    integer minimum;
    integer maximum;
} integer_range;

integer_range integer_range_create(integer minimum, integer maximum);
bool integer_range_equals(integer_range const left, integer_range const right);

typedef enum type_kind
{
    type_kind_structure,
    type_kind_function_pointer,
    type_kind_unit,
    type_kind_string_ref,
    type_kind_enumeration,
    type_kind_tuple,
    type_kind_type,
    type_kind_integer_range,
    type_kind_inferred
} type_kind;

typedef struct function_pointer function_pointer;

struct type
{
    type_kind kind;
    union
    {
        structure const *structure_;
        function_pointer const *function_pointer_;
        enumeration const *enum_;
        tuple_type const *tuple_;
        integer_range integer_range_;
        size_t inferred;
    };
};

struct function_pointer
{
    type result;
    type *arguments;
    size_t arity;
};

bool function_pointer_equals(function_pointer const left,
                             function_pointer const right);

type type_from_function_pointer(function_pointer const *value);
type type_from_unit(void);
type type_from_string_ref(void);
type type_from_enumeration(LPG_NON_NULL(enumeration const *value));
type type_from_tuple_type(tuple_type const *value);
type type_from_type(void);
type type_from_integer_range(integer_range value);
type type_from_inferred(size_t const inferred);
type *type_allocate(type const value);
bool type_equals(type const left, type const right);

function_pointer function_pointer_create(type result, type *arguments,
                                         size_t arity);
void function_pointer_free(LPG_NON_NULL(function_pointer const *value));
