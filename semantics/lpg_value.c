#include "lpg_value.h"
#include "lpg_assert.h"

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment)
{
    function_pointer_value result = {NULL, external, environment};
    return result;
}

value value_from_flat_object(value const *flat_object)
{
    value result;
    result.flat_object = flat_object;
    return result;
}

value value_from_function_pointer(function_pointer_value function_pointer)
{
    value result;
    result.function_pointer = function_pointer;
    return result;
}

value value_from_string_ref(unicode_view const string_ref)
{
    value result;
    result.string_ref = string_ref;
    return result;
}

value value_from_unit(void)
{
    value result;
    /*dummy value to avoid compiler warning*/
    result.integer_ = integer_create(0, 0);
    return result;
}

value value_from_type(type const type_)
{
    value result;
    result.type_ = type_;
    return result;
}

value value_from_enum_element(enum_element_id const element)
{
    value result;
    result.enum_element = element;
    return result;
}

value value_from_integer(integer const content)
{
    value result;
    result.integer_ = content;
    return result;
}

optional_value optional_value_create(value v)
{
    optional_value result = {true, v};
    return result;
}
