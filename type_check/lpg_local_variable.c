#include "lpg_local_variable.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_function_checking_state.h"
#include "lpg_instruction.h"
#include <lpg_source_location.h>

local_variable local_variable_create(unicode_view name, local_variable_phase phase, type const type_,
                                     optional_value compile_time_value, register_id where,
                                     struct function_checking_state *lambda_origin)
{
    local_variable const result = {name, phase, type_, compile_time_value, where, lambda_origin};
    return result;
}

void local_variable_free(local_variable const *const freed)
{
    (void)freed;
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

void local_variable_initialize(local_variable_container *variables, unicode_view name, type what,
                               optional_value compile_time_value, register_id where)
{
    LPG_FOR(size_t, i, variables->count)
    {
        local_variable *const variable = variables->elements + i;
        if (unicode_view_equals(name, variable->name))
        {
            switch (variable->phase)
            {
            case local_variable_phase_declared:
            case local_variable_phase_early_initialized:
            case local_variable_phase_lambda_being_checked:
                variable->where = where;
                variable->type_ = what;
                variable->compile_time_value = compile_time_value;
                variable->phase = local_variable_phase_initialized;
                return;

            case local_variable_phase_initialized:
                LPG_UNREACHABLE();
            }
        }
    }
    LPG_UNREACHABLE();
}

static local_variable *find_local_variable(local_variable_container *variables, unicode_view name)
{
    LPG_FOR(size_t, i, variables->count)
    {
        local_variable *const variable = variables->elements + i;
        if (unicode_view_equals(name, variable->name))
        {
            return variable;
        }
    }
    return NULL;
}

void initialize_early(local_variable_container *variables, unicode_view name, type what,
                      optional_value compile_time_value, register_id where)
{
    LPG_FOR(size_t, i, variables->count)
    {
        local_variable *const variable = variables->elements + i;
        if (unicode_view_equals(name, variable->name))
        {
            switch (variable->phase)
            {
            case local_variable_phase_declared:
                variable->where = where;
                variable->type_ = what;
                variable->compile_time_value = compile_time_value;
                variable->phase = local_variable_phase_early_initialized;
                return;

            case local_variable_phase_lambda_being_checked:
            case local_variable_phase_initialized:
            case local_variable_phase_early_initialized:
                LPG_UNREACHABLE();
            }
        }
    }
    LPG_UNREACHABLE();
}

void initialize_lambda_being_checked(local_variable_container *variables, unicode_view name, type const what,
                                     struct function_checking_state *lambda_origin)
{
    ASSUME(!local_variable_name_exists(*variables, name));
    add_local_variable(variables, local_variable_create(name, local_variable_phase_lambda_being_checked, what,
                                                        optional_value_empty, ~(register_id)0, lambda_origin));
}

bool local_variable_name_exists(local_variable_container const variables, unicode_view const name)
{
    LPG_FOR(size_t, i, variables.count)
    {
        if (unicode_view_equals(variables.elements[i].name, name))
        {
            return true;
        }
    }
    return false;
}

read_local_variable_result read_local_variable_result_create(read_local_variable_status status, variable_address where,
                                                             type what, optional_value compile_time_value, bool is_pure)
{
    read_local_variable_result const result = {status, where, what, compile_time_value, is_pure};
    return result;
}

variable_address variable_address_from_local(register_id const local)
{
    variable_address const result = {optional_capture_index_empty, local};
    return result;
}

static read_local_variable_result read_lambda_being_checked(LPG_NON_NULL(function_checking_state *const lambda_origin),
                                                            type const what)
{
    ASSUME(lambda_origin);
    register_id const into = allocate_register(&lambda_origin->used_registers);
    add_instruction(
        lambda_origin->body, instruction_create_current_function(current_function_instruction_create(into)));
    return read_local_variable_result_create(read_local_variable_status_at_address, variable_address_from_local(into),
                                             what, /*TODO*/ optional_value_empty, true);
}

read_local_variable_result read_local_variable(LPG_NON_NULL(function_checking_state *const state),
                                               unicode_view const name,
                                               source_location const original_reference_location)
{
    {
        local_variable *const existing_variable = find_local_variable(&state->local_variables, name);
        if (existing_variable)
        {
            switch (existing_variable->phase)
            {
            case local_variable_phase_declared:
                return read_local_variable_result_unknown;

            case local_variable_phase_early_initialized:
            case local_variable_phase_initialized:
                return read_local_variable_result_create(
                    read_local_variable_status_at_address, variable_address_from_local(existing_variable->where),
                    existing_variable->type_, existing_variable->compile_time_value, true);

            case local_variable_phase_lambda_being_checked:
                return read_lambda_being_checked(existing_variable->lambda_origin, existing_variable->type_);
            }
        }
    }
    if (!state->parent)
    {
        return read_local_variable_result_unknown;
    }
    read_local_variable_result const outer_variable =
        read_local_variable(state->parent, name, original_reference_location);
    switch (outer_variable.status)
    {
    case read_local_variable_status_forbidden:
    case read_local_variable_status_unknown:
        return outer_variable;

    case read_local_variable_status_at_address:
        break;

    case read_local_variable_status_compile_time_value:
        return outer_variable;
    }
    if (outer_variable.compile_time_value.is_set)
    {
        return read_local_variable_result_create(read_local_variable_status_compile_time_value,
                                                 variable_address_from_local(~(register_id)0), outer_variable.what,
                                                 outer_variable.compile_time_value, outer_variable.is_pure);
    }
    if (!state->may_capture_runtime_variables)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_cannot_capture_runtime_variable, original_reference_location));
        return read_local_variable_result_forbidden;
    }
    capture_index const existing_capture = require_capture(state, outer_variable.where, outer_variable.what);
    return read_local_variable_result_create(read_local_variable_status_at_address,
                                             variable_address_from_capture(existing_capture), outer_variable.what,
                                             outer_variable.compile_time_value, outer_variable.is_pure);
}
