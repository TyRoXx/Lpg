#include <lpg_allocate.h>
#include "lpg_captures.h"
#include "function_checking_state.h"

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
        return right.has_value && (left.value == right.value);
    }
    return !right.has_value;
}

bool variable_address_equals(variable_address const left, variable_address const right)
{
    return optional_capture_index_equals(left.captured_in_current_lambda, right.captured_in_current_lambda) &&
           (left.local_address == right.local_address);
}

capture_index require_capture(LPG_NON_NULL(function_checking_state *const state), variable_address const captured,
                              type const what)
{
    for (capture_index i = 0; i < state->capture_count; ++i)
    {
        if (variable_address_equals(state->captures[i].from, captured))
        {
            return i;
        }
    }
    capture_index const result = state->capture_count;
    state->captures = reallocate_array(state->captures, (state->capture_count + 1), sizeof(*state->captures));
    state->captures[state->capture_count] = capture_create(captured, what);
    ++(state->capture_count);
    return result;
}
