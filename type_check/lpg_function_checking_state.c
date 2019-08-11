#include "lpg_function_checking_state.h"
#include "lpg_allocate.h"

void function_checking_state_free(function_checking_state const freed)
{
    local_variable_container_free(freed.local_variables);
    if (freed.register_compile_time_values)
    {
        deallocate(freed.register_compile_time_values);
    }
}

optional_value read_register_compile_time_value(function_checking_state const *const state, register_id const which)
{
    if (which >= state->register_compile_time_value_count)
    {
        return optional_value_empty;
    }
    return state->register_compile_time_values[which];
}

void write_register_compile_time_value(function_checking_state *const state, register_id const which,
                                       value const compile_time_value)
{
    if (which < state->register_compile_time_value_count)
    {
        ASSERT(!state->register_compile_time_values[which].is_set);
        state->register_compile_time_values[which] = optional_value_create(compile_time_value);
        return;
    }
    ASSUME(which != ~(register_id)0);
    ASSUME(state->register_compile_time_values || (state->register_compile_time_value_count == 0));
    state->register_compile_time_values = reallocate_array_exponentially(
        state->register_compile_time_values, (which + 1), sizeof(*state->register_compile_time_values),
        state->register_compile_time_value_count, &state->register_compile_time_value_capacity);
    ASSUME(state->register_compile_time_values);
    for (register_id i = state->register_compile_time_value_count; i < which; ++i)
    {
        state->register_compile_time_values[i] = optional_value_empty;
    }
    state->register_compile_time_values[which] = optional_value_create(compile_time_value);
    state->register_compile_time_value_count = (which + 1);
}

void emit_semantic_error(function_checking_state const *const state, semantic_error const error)
{
    state->on_error(complete_semantic_error_create(error, source_file_from_owning(*state->source)), state->user);
}
