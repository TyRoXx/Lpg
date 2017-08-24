#include "lpg_value.h"
#include "lpg_assert.h"

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment)
{
    function_pointer_value result = {0, external, environment};
    return result;
}

function_pointer_value
function_pointer_value_from_internal(function_id const code)
{
    function_pointer_value result = {code, NULL, NULL};
    return result;
}

bool function_pointer_value_equals(function_pointer_value const left,
                                   function_pointer_value const right)
{
    if (left.external)
    {
        if (right.external)
        {
            return (left.external == right.external) &&
                   (left.external_environment == right.external_environment);
        }
        return false;
    }
    if (right.external)
    {
        return false;
    }
    return (left.code == right.code);
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

value value_from_enum_element(enum_element_id const element, value *const state)
{
    value result;
    result.kind = value_kind_enum_element;
    result.enum_element.which = element;
    result.enum_element.state = state;
    return result;
}

value value_from_integer(integer const content)
{
    value result;
    result.kind = value_kind_integer;
    result.integer_ = content;
    return result;
}

value value_from_tuple(value_tuple content)
{
    value result;
    result.kind = value_kind_tuple;
    result.tuple_ = content;
    return result;
}

value value_from_enum_constructor(void)
{
    value result;
    result.kind = value_kind_enum_constructor;
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
        return function_pointer_value_equals(
            left.function_pointer, right.function_pointer);

    case value_kind_flat_object:
        LPG_TO_DO();

    case value_kind_type:
        LPG_TO_DO();

    case value_kind_enum_element:
    {
        if (left.enum_element.which != right.enum_element.which)
        {
            return false;
        }
        value const left_state = left.enum_element.state
                                     ? *left.enum_element.state
                                     : value_from_unit();
        value const right_state = right.enum_element.state
                                      ? *right.enum_element.state
                                      : value_from_unit();
        return value_equals(left_state, right_state);
    }

    case value_kind_unit:
        return true;

    case value_kind_tuple:
        if (left.tuple_.element_count != right.tuple_.element_count)
        {
            return false;
        }
        for (size_t i = 0; i < left.tuple_.element_count; ++i)
        {
            if (!value_equals(
                    left.tuple_.elements[i], right.tuple_.elements[i]))
            {
                return false;
            }
        }
        return true;

    case value_kind_enum_constructor:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

optional_value optional_value_create(value v)
{
    optional_value result = {true, v};
    return result;
}
