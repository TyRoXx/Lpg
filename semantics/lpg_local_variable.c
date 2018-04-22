#include <lpg_source_location.h>
#include "lpg_local_variable.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_instruction.h"
#include "lpg_function_checking_state.h"

local_variable local_variable_create(unicode_string name, type const type_, optional_value compile_time_value,
                                     register_id where)
{
    local_variable const result = {name, type_, compile_time_value, where};
    return result;
}

void local_variable_free(local_variable const *const value)
{
    unicode_string_free(&value->name);
}

void local_variable_container_free(local_variable_container const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        local_variable_free(freed.elements + i);
    }
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

void add_local_variable(local_variable_container *to, local_variable variable)
{
    to->elements = reallocate_array(to->elements, to->count + 1, sizeof(*to->elements));
    to->elements[to->count] = variable;
    ++(to->count);
}

bool local_variable_name_exists(local_variable_container const variables, unicode_view const name)
{
    LPG_FOR(size_t, i, variables.count)
    {
        if (unicode_view_equals(unicode_view_from_string(variables.elements[i].name), name))
        {
            return true;
        }
    }
    return false;
}

read_local_variable_result read_local_variable_result_create(variable_address where, type what,
                                                             optional_value compile_time_value, bool is_pure)
{
    read_local_variable_result const result = {read_local_variable_status_ok, where, what, compile_time_value, is_pure};
    return result;
}

variable_address variable_address_from_local(register_id const local)
{
    variable_address const result = {optional_capture_index_empty, local};
    return result;
}

read_local_variable_result read_local_variable(LPG_NON_NULL(function_checking_state *const state),
                                               instruction_sequence *const sequence, unicode_view const name,
                                               source_location const original_reference_location)
{
    LPG_FOR(size_t, i, state->local_variables.count)
    {
        local_variable const *const variable = state->local_variables.elements + i;
        if (unicode_view_equals(unicode_view_from_string(variable->name), name))
        {
            return read_local_variable_result_create(
                variable_address_from_local(variable->where), variable->type_, variable->compile_time_value, true);
        }
    }
    if (!state->parent)
    {
        return read_local_variable_result_unknown;
    }
    read_local_variable_result const outer_variable =
        read_local_variable(state->parent, NULL, name, original_reference_location);
    switch (outer_variable.status)
    {
    case read_local_variable_status_forbidden:
    case read_local_variable_status_unknown:
        return outer_variable;

    case read_local_variable_status_ok:
        break;
    }
    if (outer_variable.compile_time_value.is_set && sequence)
    {
        register_id const where = allocate_register(&state->used_registers);
        add_instruction(sequence, instruction_create_literal(literal_instruction_create(
                                      where, outer_variable.compile_time_value.value_, outer_variable.what)));
        write_register_compile_time_value(state, where, outer_variable.compile_time_value.value_);
        return read_local_variable_result_create(variable_address_from_local(where), outer_variable.what,
                                                 outer_variable.compile_time_value, outer_variable.is_pure);
    }
    if (!state->may_capture_runtime_variables)
    {
        state->on_error(
            semantic_error_create(semantic_error_cannot_capture_runtime_variable, original_reference_location),
            state->user);
        return read_local_variable_result_forbidden;
    }
    capture_index const existing_capture = require_capture(state, outer_variable.where, outer_variable.what);
    return read_local_variable_result_create(variable_address_from_capture(existing_capture), outer_variable.what,
                                             outer_variable.compile_time_value, outer_variable.is_pure);
}
