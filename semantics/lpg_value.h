#pragma once
#include "lpg_checked_function.h"
#include "lpg_integer.h"
#include "lpg_type.h"
#include "lpg_garbage_collector.h"
#include "lpg_function_id.h"

typedef struct enumeration enumeration;

typedef struct value external_function(struct value const *,
                                       struct value const *,
                                       LPG_NON_NULL(garbage_collector *),
                                       void *);

typedef struct function_pointer_value
{
    function_id code;
    external_function *external;
    void *external_environment;
    struct value *captures;
    size_t capture_count;
} function_pointer_value;

function_pointer_value
function_pointer_value_from_external(LPG_NON_NULL(external_function *external),
                                     void *environment);
function_pointer_value
function_pointer_value_from_internal(function_id const code,
                                     struct value *const captures,
                                     size_t const capture_count);
bool function_pointer_value_equals(function_pointer_value const left,
                                   function_pointer_value const right);

typedef enum value_kind
{
    value_kind_integer,
    value_kind_string,
    value_kind_function_pointer,
    value_kind_flat_object,
    value_kind_type,
    value_kind_enum_element,
    value_kind_unit,
    value_kind_tuple,
    value_kind_enum_constructor
} value_kind;

typedef struct value_tuple
{
    struct value *elements;
    size_t element_count;
} value_tuple;

value_tuple value_tuple_create(struct value *elements, size_t element_count);

typedef struct enum_element_value
{
    enum_element_id which;

    /*NULL means unit*/
    struct value *state;
} enum_element_value;

typedef struct value
{
    value_kind kind;
    union
    {
        struct value const *flat_object;
        integer integer_;
        unicode_view string_ref;
        function_pointer_value function_pointer;
        type type_;
        enum_element_value enum_element;
        value_tuple tuple_;
    };
} value;

value value_from_flat_object(LPG_NON_NULL(value const *flat_object));
value value_from_function_pointer(function_pointer_value function_pointer);
value value_from_string_ref(unicode_view string_ref);
value value_from_unit(void);
value value_from_type(type type_);
value value_from_enum_element(enum_element_id element, value *state);
value value_from_integer(integer content);
value value_from_tuple(value_tuple content);
value value_from_enum_constructor(void);
bool value_equals(value left, value right);
bool value_less_than(value left, value right);
bool value_greater_than(value left, value right);

typedef struct optional_value
{
    bool is_set;
    value value_;
} optional_value;

optional_value optional_value_create(value v);

static optional_value const optional_value_empty = {
    false, {value_kind_integer, {NULL}}};
