#pragma once
#include "lpg_checked_function.h"
#include "lpg_integer.h"
#include "lpg_type.h"

typedef struct enumeration enumeration;

typedef union value external_function(union value const *, void *);

typedef struct function_pointer_value
{
    checked_function const *code;
    external_function *external;
    void *external_environment;
} function_pointer_value;

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment);

typedef union value
{
    integer integer_;
    unicode_view string_ref;
    function_pointer_value function_pointer;
    union value const *flat_object;
    type const *type_;
} value;

value value_from_flat_object(value const *flat_object);
value value_from_function_pointer(function_pointer_value function_pointer);
value value_from_string_ref(unicode_view const string_ref);
value value_from_unit(void);
value value_from_type(type const *type_);

typedef struct optional_value
{
    bool is_set;
    value value_;
} optional_value;

optional_value optional_value_create(value v);

static optional_value const optional_value_empty = {false, {{0, 0}}};
