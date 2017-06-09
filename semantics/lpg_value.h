#pragma once
#include "lpg_checked_function.h"
#include "lpg_integer.h"
#include "lpg_type.h"
#include "lpg_garbage_collector.h"

typedef struct enumeration enumeration;

typedef struct value external_function(struct value const *,
                                       struct value const *,
                                       garbage_collector *, void *);

typedef struct function_pointer_value
{
    checked_function const *code;
    external_function *external;
    void *external_environment;
} function_pointer_value;

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment);

typedef enum value_kind
{
    value_kind_integer,
    value_kind_string,
    value_kind_function_pointer,
    value_kind_flat_object,
    value_kind_type,
    value_kind_enum_element
} value_kind;

typedef struct value
{
    value_kind kind;
    union
    {
        integer integer_;
        unicode_view string_ref;
        function_pointer_value function_pointer;
        struct value const *flat_object;
        type type_;
        enum_element_id enum_element;
    };
} value;

value value_from_flat_object(value const *flat_object);
value value_from_function_pointer(function_pointer_value function_pointer);
value value_from_string_ref(unicode_view const string_ref);
value value_from_unit(void);
value value_from_type(type const type_);
value value_from_enum_element(enum_element_id const element);
value value_from_integer(integer const content);
bool value_equals(value const left, value const right);

typedef struct optional_value
{
    bool is_set;
    value value_;
} optional_value;

optional_value optional_value_create(value v);

static optional_value const optional_value_empty = {
    false, {value_kind_integer, {{0, 0}}}};
