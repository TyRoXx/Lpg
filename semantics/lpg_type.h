#pragma once

#include "lpg_unicode_string.h"
#include "lpg_struct_member_id.h"
#include "lpg_enum_element_id.h"
#include "lpg_integer.h"
#include "lpg_garbage_collector.h"
#include "lpg_function_id.h"

typedef struct structure_member structure_member;

typedef struct structure
{
    structure_member *members;
    struct_member_id count;
} structure;

structure structure_create(structure_member *members, struct_member_id count);
void structure_free(structure const *value);

typedef struct type type;

typedef struct enumeration_element enumeration_element;

typedef struct enumeration
{
    enumeration_element *elements;
    enum_element_id size;
} enumeration;

enumeration enumeration_create(enumeration_element *elements, enum_element_id size);
void enumeration_free(LPG_NON_NULL(enumeration const *value));

typedef struct tuple_type
{
    type *elements;
    size_t length;
} tuple_type;

tuple_type tuple_type_create(type *elements, size_t length);
bool tuple_type_equals(tuple_type const left, tuple_type const right);

typedef struct method_description method_description;

typedef struct interface
{
    method_description *methods;
    size_t method_count;
} interface;

interface interface_create(method_description *methods, size_t method_count);
void interface_free(interface const value);

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
    type_kind_inferred,
    type_kind_enum_constructor,
    type_kind_lambda,
    type_kind_interface
} type_kind;

typedef struct function_pointer function_pointer;

typedef struct enum_constructor_type enum_constructor_type;

typedef struct lambda_type
{
    function_id lambda;
} lambda_type;

lambda_type lambda_type_create(function_id const lambda);

struct type
{
    type_kind kind;
    union
    {
        structure const *structure_;
        function_pointer const *function_pointer_;
        enumeration const *enum_;
        tuple_type tuple_;
        integer_range integer_range_;
        size_t inferred;
        enum_constructor_type *enum_constructor;
        lambda_type lambda;
        interface const *interface_;
    };
};

struct method_description
{
    unicode_string name;
    tuple_type parameters;
    type result;
};

method_description method_description_create(unicode_string name, tuple_type parameters, type result);
void method_description_free(method_description const value);

struct enum_constructor_type
{
    enumeration const *enumeration;
    enum_element_id which;
};

struct enumeration_element
{
    unicode_string name;
    type state;
};

enumeration_element enumeration_element_create(unicode_string name, type state);
void enumeration_element_free(LPG_NON_NULL(enumeration_element const *value));

struct function_pointer
{
    type result;
    tuple_type parameters;
    tuple_type captures;
};

function_pointer function_pointer_create(type result, tuple_type parameters, tuple_type captures);
bool function_pointer_equals(function_pointer const left, function_pointer const right);

type type_from_function_pointer(function_pointer const *value);
type type_from_unit(void);
type type_from_string_ref(void);
type type_from_enumeration(LPG_NON_NULL(enumeration const *value));
type type_from_tuple_type(tuple_type const value);
type type_from_type(void);
type type_from_integer_range(integer_range value);
type type_from_inferred(size_t const inferred);
type type_from_enum_constructor(LPG_NON_NULL(enum_constructor_type *enum_constructor));
type type_from_lambda(lambda_type const lambda);
type type_from_interface(LPG_NON_NULL(interface const *value));
type *type_allocate(type const value);
bool type_equals(type const left, type const right);
type type_clone(type const original, garbage_collector *const clone_gc);

void function_pointer_free(LPG_NON_NULL(function_pointer const *value));
