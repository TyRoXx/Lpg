#pragma once
#include "lpg_check.h"

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
} value;

value value_from_flat_object(value const *flat_object);
value value_from_function_pointer(function_pointer_value function_pointer);
value value_from_string_ref(unicode_view const string_ref);
value value_from_unit(void);

void interprete(checked_program const program, value const *globals);
