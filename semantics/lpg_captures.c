#include "lpg_captures.h"

capture capture_create(variable_address const from, type const what)
{
    capture const result = {from, what};
    return result;
}

variable_address variable_address_from_capture(capture_index const captured)
{
    variable_address const result = {{true, captured}, 0};
    return result;
}

static bool optional_capture_index_equals(optional_capture_index const left, optional_capture_index const right)
{
    if (left.has_value)
    {
        if (right.has_value)
        {
            return (left.value == right.value);
        }
        return false;
    }
    if (right.has_value)
    {
        return false;
    }
    return true;
}

bool variable_address_equals(variable_address const left, variable_address const right)
{
    return optional_capture_index_equals(left.captured_in_current_lambda, right.captured_in_current_lambda) &&
           (left.local_address == right.local_address);
}