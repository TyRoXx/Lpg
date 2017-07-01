#include "lpg_value.h"
#include "lpg_assert.h"

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment)
{
    function_pointer_value result = {NULL, external, environment};
    return result;
}

function_pointer_value
function_pointer_value_from_internal(checked_function const *code)
{
    function_pointer_value result = {code, NULL, NULL};
    return result;
}

value value_from_flat_object(value const *flat_object)
{
    value result;
    result.kind = value_kind_flat_object;
    result.flat_object = flat_object;
    return result;
}

value value_from_function_pointer(function_pointer_value function_pointer)
{
    value result;
    result.kind = value_kind_function_pointer;
    result.function_pointer = function_pointer;
    return result;
}

value value_from_string_ref(unicode_view const string_ref)
{
    value result;
    result.kind = value_kind_string;
    result.string_ref = string_ref;
    return result;
}

value value_from_unit(void)
{
    value result;
    result.kind = value_kind_unit;
    /*dummy value to avoid compiler warning*/
    result.integer_ = integer_create(0, 0);
    return result;
}

value value_from_type(type const type_)
{
    value result;
    result.kind = value_kind_type;
    result.type_ = type_;
    return result;
}

value value_from_enum_element(enum_element_id const element)
{
    value result;
    result.kind = value_kind_enum_element;
    result.enum_element = element;
    return result;
}

value value_from_integer(integer const content)
{
    value result;
    result.kind = value_kind_integer;
    result.integer_ = content;
    return result;
}

bool value_equals(value const left, value const right)
{
    if (left.kind != right.kind)
    {
        return false;
    }
    switch (left.kind)
    {
    case value_kind_integer:
        return integer_equal(left.integer_, right.integer_);

    case value_kind_string:
        return unicode_view_equals(left.string_ref, right.string_ref);

    case value_kind_function_pointer:
        LPG_TO_DO();

    case value_kind_flat_object:
        LPG_TO_DO();

    case value_kind_type:
        LPG_TO_DO();

    case value_kind_enum_element:
        return (left.enum_element == right.enum_element);

    case value_kind_unit:
        return true;
    }
    UNREACHABLE();
}

optional_value optional_value_create(value v)
{
    optional_value result = {true, v};
    return result;
}
