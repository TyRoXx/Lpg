#include "lpg_check.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_function_checking_state.h"
#include "lpg_instruction.h"
#include "lpg_instruction_checkpoint.h"
#include "lpg_integer_set.h"
#include "lpg_interpret.h"
#include "lpg_local_variable.h"
#include "lpg_standard_library.h"
#include "lpg_string_literal.h"
#include "lpg_structure_member.h"
#include "lpg_value.h"
#include <string.h>

check_function_result check_function_result_create(checked_function function, capture *captures, size_t capture_count)
{
    check_function_result const result = {true, function, captures, capture_count};
    return result;
}

check_function_result const check_function_result_empty = {false, {NULL, {NULL, 0}, NULL, 0}, NULL, 0};

typedef enum evaluation_status {
    evaluation_status_value = 1,
    evaluation_status_return,
    evaluation_status_exit,
    evaluation_status_error
} evaluation_status;

typedef struct evaluate_expression_result
{
    evaluation_status status;
    type type_;
    optional_value compile_time_value;
    register_id where;
    bool is_pure;
} evaluate_expression_result;

static evaluate_expression_result evaluate_expression_result_create(evaluation_status const status,
                                                                    register_id const where, type const type_,
                                                                    optional_value const compile_time_value,
                                                                    bool const is_pure)
{
    evaluate_expression_result const result = {status, type_, compile_time_value, where, is_pure};
    return result;
}

static evaluate_expression_result const evaluate_expression_result_empty = {
    evaluation_status_error, {type_kind_type, {0}}, {false, {value_kind_integer, {{NULL, 0}}}}, 0, false};

static function_checking_state function_checking_state_create(
    program_check *const root, function_checking_state *const parent, bool const may_capture_runtime_variables,
    structure const *global, check_error_handler *const on_error, void *const user, checked_program *const program,
    instruction_sequence *const body, optional_type const return_type, bool const has_declared_return_type,
    source_file_owning const *const source, unicode_view const current_import_directory)
{
    function_checking_state const result = {root,
                                            parent,
                                            may_capture_runtime_variables,
                                            NULL,
                                            0,
                                            0,
                                            NULL,
                                            0,
                                            false,
                                            global,
                                            on_error,
                                            {NULL, 0},
                                            user,
                                            program,
                                            body,
                                            return_type,
                                            has_declared_return_type,
                                            NULL,
                                            0,
                                            source,
                                            current_import_directory};
    return result;
}

typedef struct optional_enum_id
{
    bool is_set;
    enum_id value;
} optional_enum_id;

static optional_enum_id optional_enum_id_create_empty(void)
{
    optional_enum_id const result = {false, ~(enum_id)0};
    return result;
}

static optional_enum_id optional_enum_id_create_set(enum_id const value_)
{
    optional_enum_id const result = {true, value_};
    return result;
}

typedef struct type_expectation
{
    optional_type exact_type;
    optional_enum_id enum_constructors;
} type_expectation;

static type_expectation type_expectation_create_exact(optional_type exact_type)
{
    type_expectation result;
    result.exact_type = exact_type;
    result.enum_constructors = optional_enum_id_create_empty();
    return result;
}

static type_expectation type_expectation_create_enum_constructors(enum_id const id)
{
    type_expectation result;
    result.exact_type = optional_type_create_empty();
    result.enum_constructors = optional_enum_id_create_set(id);
    return result;
}

static evaluate_expression_result check_sequence(function_checking_state *const state,
                                                 instruction_sequence *const output, sequence const input,
                                                 type_expectation const expected_return_type);

static type get_parameter_type(type const callee, size_t const which_parameter,
                               checked_function const *const all_functions, enumeration const *const all_enums)
{
    switch (callee.kind)
    {
    case type_kind_generic_struct:
        LPG_TO_DO();

    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_lambda:
        ASSUME(which_parameter < all_functions[callee.lambda.lambda].signature->parameters.length);
        return all_functions[callee.lambda.lambda].signature->parameters.elements[which_parameter];

    case type_kind_function_pointer:
        ASSUME(which_parameter < callee.function_pointer_->parameters.length);
        return callee.function_pointer_->parameters.elements[which_parameter];

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_UNREACHABLE();

    case type_kind_enum_constructor:
    {
        ASSUME(which_parameter == 0);
        optional_type const state =
            all_enums[callee.enum_constructor->enumeration].elements[callee.enum_constructor->which].state;
        ASSUME(state.is_set);
        return state.value;
    }
    }
    LPG_UNREACHABLE();
}

typedef struct read_structure_element_result
{
    optional_value compile_time_value;
    type type_;
    bool success;
    bool is_pure;
} read_structure_element_result;

static read_structure_element_result read_structure_element_result_create(bool const success, type const type_,
                                                                          optional_value const compile_time_value,
                                                                          bool const is_pure)
{
    read_structure_element_result const result = {compile_time_value, type_, success, is_pure};
    return result;
}

static read_structure_element_result
read_structure_element(function_checking_state *const state, instruction_sequence *const function,
                       structure const *const structure_type, register_id const where, unicode_view const element_name,
                       source_location const element_name_location, register_id const result)
{
    optional_value const compile_time_value = read_register_compile_time_value(state, where);
    LPG_FOR(struct_member_id, i, structure_type->count)
    {
        if (!unicode_view_equals(element_name, unicode_view_from_string(structure_type->members[i].name)))
        {
            continue;
        }
        if (compile_time_value.is_set)
        {
            ASSERT(compile_time_value.value_.kind == value_kind_structure);
            ASSUME(i < compile_time_value.value_.structure.count);
            value const element_value = compile_time_value.value_.structure.members[i];
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          result, element_value, structure_type->members[i].what)));
            write_register_compile_time_value(state, result, element_value);
            return read_structure_element_result_create(
                true, structure_type->members[i].what, optional_value_create(element_value), true);
        }
        add_instruction(function, instruction_create_read_struct(read_struct_instruction_create(where, i, result)));
        return read_structure_element_result_create(
            true, structure_type->members[i].what, structure_type->members[i].compile_time_value, true);
    }
    emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element_name_location));
    return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
}

static read_structure_element_result read_tuple_element(function_checking_state *state, instruction_sequence *function,
                                                        tuple_type const read_from, register_id const where,
                                                        unicode_view const element_name,
                                                        source_location const element_name_location,
                                                        register_id const result)
{
    integer element_index;
    if (!integer_parse(&element_index, element_name))
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element_name_location));
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
    }
    if (!integer_less(element_index, integer_create(0, read_from.length)))
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element_name_location));
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
    }
    ASSERT(integer_less(element_index, integer_create(0, ~(struct_member_id)0)));
    struct_member_id const element = (struct_member_id)element_index.low;
    add_instruction(function, instruction_create_read_struct(read_struct_instruction_create(where, element, result)));
    optional_value const compile_time_tuple = read_register_compile_time_value(state, where);
    if (compile_time_tuple.is_set)
    {
        ASSUME(element < compile_time_tuple.value_.tuple_.element_count);
        value const compile_time_element = compile_time_tuple.value_.tuple_.elements[element];
        write_register_compile_time_value(state, result, compile_time_element);
        return read_structure_element_result_create(
            true, read_from.elements[element], optional_value_create(compile_time_element), true);
    }
    return read_structure_element_result_create(true, read_from.elements[element], optional_value_empty, true);
}

static evaluate_expression_result evaluate_expression(function_checking_state *const state,
                                                      instruction_sequence *const function, expression const element,
                                                      unicode_view const *const early_initialized_variable,
                                                      type_expectation const expected_result_type);

static evaluate_expression_result evaluate_return_expression(function_checking_state *state,
                                                             instruction_sequence *sequence,
                                                             const expression *expression);

static read_structure_element_result read_interface_element_at(function_checking_state *state,
                                                               instruction_sequence *function, register_id const from,
                                                               interface_id const from_type,
                                                               function_id const method_index, register_id const result)
{
    add_instruction(
        function, instruction_create_get_method(get_method_instruction_create(from_type, from, method_index, result)));
    function_pointer *const method_object = garbage_collector_allocate(&state->program->memory, sizeof(*method_object));
    type *const captures = garbage_collector_allocate_array(&state->program->memory, 1, sizeof(*captures));
    captures[0] = type_from_interface(from_type);
    if (!state->root->interfaces_defined[from_type])
    {
        LPG_TO_DO();
    }
    method_description const method = state->program->interfaces[from_type].methods[method_index];
    *method_object = function_pointer_create(optional_type_create_set(method.result), method.parameters,
                                             tuple_type_create(captures, 1), optional_type_create_empty());
    return read_structure_element_result_create(
        true, type_from_function_pointer(method_object), optional_value_empty, false);
}

static read_structure_element_result
read_interface_element(function_checking_state *state, instruction_sequence *function, register_id const from,
                       interface_id const from_type, unicode_view const element_name,
                       source_location const element_source, register_id const result)
{
    if (!state->root->interfaces_defined[from_type])
    {
        LPG_TO_DO();
    }
    lpg_interface *const from_interface = state->program->interfaces + from_type;
    for (function_id i = 0; i < from_interface->method_count; ++i)
    {
        if (unicode_view_equals(unicode_view_from_string(from_interface->methods[i].name), element_name))
        {
            return read_interface_element_at(state, function, from, from_type, i, result);
        }
    }
    emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element_source));
    return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
}

static read_structure_element_result read_element(function_checking_state *state, instruction_sequence *function,
                                                  expression const object_tree,
                                                  const identifier_expression *const element, register_id const result)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, function);
    evaluate_expression_result const object = evaluate_expression(
        state, function, object_tree, NULL, type_expectation_create_exact(optional_type_create_empty()));
    switch (object.status)
    {
    case evaluation_status_error:
    case evaluation_status_return:
    case evaluation_status_exit:
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case evaluation_status_value:
        break;
    }

    type const *const actual_type = &object.type_;
    switch (actual_type->kind)
    {
    case type_kind_generic_struct:
    case type_kind_host_value:
    case type_kind_generic_lambda:
    case type_kind_method_pointer:
        emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element->source));
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case type_kind_structure:
        return read_structure_element(state, function, state->program->structs + actual_type->structure_, object.where,
                                      unicode_view_from_string(element->value), element->source, result);

    case type_kind_interface:
        return read_interface_element(state, function, object.where, actual_type->interface_,
                                      unicode_view_from_string(element->value), element->source, result);

    case type_kind_enum_constructor:
    case type_kind_integer_range:
    case type_kind_lambda:
    case type_kind_unit:
    case type_kind_function_pointer:
    case type_kind_string:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element->source));
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case type_kind_enumeration:
        emit_semantic_error(state, semantic_error_create(semantic_error_no_members_on_enum_elements, element->source));
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case type_kind_tuple:
        return read_tuple_element(state, function, actual_type->tuple_, object.where,
                                  unicode_view_from_string(element->value), element->source, result);

    case type_kind_type:
    {
        if (!object.compile_time_value.is_set)
        {
            restore(previous_code);
            emit_semantic_error(
                state, semantic_error_create(semantic_error_expected_compile_time_type, element->source));
            return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
        }
        type const left_side_type = object.compile_time_value.value_.type_;
        switch (left_side_type.kind)
        {
        case type_kind_generic_struct:
            LPG_TO_DO();

        case type_kind_string:
        case type_kind_unit:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_interface:
        case type_kind_structure:
        case type_kind_function_pointer:
        case type_kind_tuple:
        case type_kind_lambda:
        case type_kind_enum_constructor:
        case type_kind_method_pointer:
        case type_kind_generic_enum:
        case type_kind_generic_interface:
        case type_kind_host_value:
        case type_kind_generic_lambda:
            emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element->source));
            return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

        case type_kind_enumeration:
        {
            enumeration const *const enum_ = state->program->enums + left_side_type.enum_;
            restore(previous_code);
            LPG_FOR(enum_element_id, i, enum_->size)
            {
                if (unicode_view_equals(
                        unicode_view_from_string(element->value), unicode_view_from_string(enum_->elements[i].name)))
                {
                    if (!enum_->elements[i].state.is_set)
                    {
                        value const literal = value_from_enum_element(i, enum_->elements[i].state.value, NULL);
                        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                      result, literal, type_from_enumeration(left_side_type.enum_))));
                        write_register_compile_time_value(state, result, literal);
                        return read_structure_element_result_create(
                            true, left_side_type, optional_value_create(literal), true);
                    }
                    value const literal = value_from_enum_constructor();
                    enum_constructor_type *const constructor_type =
                        garbage_collector_allocate(&state->program->memory, sizeof(*constructor_type));
                    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                  result, literal, type_from_enum_constructor(constructor_type))));
                    write_register_compile_time_value(state, result, literal);
                    constructor_type->enumeration = left_side_type.enum_;
                    constructor_type->which = i;
                    return read_structure_element_result_create(
                        true, type_from_enum_constructor(constructor_type), optional_value_create(literal), true);
                }
            }
            emit_semantic_error(state, semantic_error_create(semantic_error_unknown_element, element->source));
            return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
        }
        }
        LPG_UNREACHABLE();
    }
    }
    LPG_UNREACHABLE();
}

static size_t expected_call_argument_count(const type callee, checked_function const *const all_functions,
                                           enumeration const *const all_enums)
{
    switch (callee.kind)
    {
    case type_kind_generic_struct:
        LPG_TO_DO();

    case type_kind_lambda:
        return all_functions[callee.lambda.lambda].signature->parameters.length;

    case type_kind_function_pointer:
        return callee.function_pointer_->parameters.length;

    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_enum_constructor:
    {
        enumeration const *const enum_ = all_enums + callee.enum_constructor->enumeration;
        ASSUME(callee.enum_constructor->which < enum_->size);
        return enum_->elements[callee.enum_constructor->which].state.is_set ? 1 : 0;
    }
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result read_variable(LPG_NON_NULL(function_checking_state *const state),
                                                LPG_NON_NULL(instruction_sequence *const to), unicode_view const name,
                                                source_location const where,
                                                type_expectation const expected_result_type)
{
    ASSUME(to);
    instruction_checkpoint const previous_code = make_checkpoint(state, to);
    read_local_variable_result const local = read_local_variable(state, name, where);
    switch (local.status)
    {
    case read_local_variable_status_at_address:
        if (local.where.captured_in_current_lambda.has_value)
        {
            register_id const captures = allocate_register(&state->used_registers);
            add_instruction(to, instruction_create_get_captures(captures));
            register_id const capture_result = allocate_register(&state->used_registers);
            add_instruction(to, instruction_create_read_struct(read_struct_instruction_create(
                                    captures, local.where.captured_in_current_lambda.value, capture_result)));
            return evaluate_expression_result_create(
                evaluation_status_value, capture_result, local.what, local.compile_time_value, local.is_pure);
        }
        return evaluate_expression_result_create(
            evaluation_status_value, local.where.local_address, local.what, local.compile_time_value, local.is_pure);

    case read_local_variable_status_compile_time_value:
    {
        register_id const into = allocate_register(&state->used_registers);
        ASSUME(local.compile_time_value.is_set);
        add_instruction(to, instruction_create_literal(
                                literal_instruction_create(into, local.compile_time_value.value_, local.what)));
        write_register_compile_time_value(state, into, local.compile_time_value.value_);
        return evaluate_expression_result_create(
            evaluation_status_value, into, local.what, local.compile_time_value, local.is_pure);
    }

    case read_local_variable_status_unknown:
        break;

    case read_local_variable_status_forbidden:
        return evaluate_expression_result_empty;
    }

    if (expected_result_type.exact_type.is_set && (expected_result_type.exact_type.value.kind == type_kind_enumeration))
    {
        enumeration const *const enum_ = state->program->enums + expected_result_type.exact_type.value.enum_;
        for (enum_element_id i = 0; i < enum_->size; ++i)
        {
            enumeration_element const *const element = enum_->elements + i;
            if (unicode_view_equals(name, unicode_view_from_string(element->name)))
            {
                if (element->state.is_set)
                {
                    LPG_TO_DO();
                }
                value const literal = value_from_enum_element(i, enum_->elements[i].state.value, NULL);
                register_id const result = allocate_register(&state->used_registers);
                add_instruction(to, instruction_create_literal(literal_instruction_create(
                                        result, literal, expected_result_type.exact_type.value)));
                write_register_compile_time_value(state, result, literal);
                return evaluate_expression_result_create(evaluation_status_value, result,
                                                         expected_result_type.exact_type.value,
                                                         optional_value_create(literal), true);
            }
        }
    }

    if (expected_result_type.enum_constructors.is_set)
    {
        enumeration const *const enum_ = state->program->enums + expected_result_type.enum_constructors.value;
        for (enum_element_id i = 0; i < enum_->size; ++i)
        {
            enumeration_element const *const element = enum_->elements + i;
            if (!unicode_view_equals(name, unicode_view_from_string(element->name)))
            {
                continue;
            }
            if (!element->state.is_set)
            {
                LPG_TO_DO();
            }
            enum_constructor_type *const enum_constructor =
                garbage_collector_allocate(&state->program->memory, sizeof(*enum_constructor));
            *enum_constructor = enum_constructor_type_create(expected_result_type.enum_constructors.value, i);
            type const constructor_type = type_from_enum_constructor(enum_constructor);
            value const literal = value_from_enum_constructor();
            register_id const result = allocate_register(&state->used_registers);
            add_instruction(
                to, instruction_create_literal(literal_instruction_create(result, literal, constructor_type)));
            write_register_compile_time_value(state, result, literal);
            return evaluate_expression_result_create(
                evaluation_status_value, result, constructor_type, optional_value_create(literal), true);
        }
    }

    register_id const global = allocate_register(&state->used_registers);
    add_instruction(to, instruction_create_global(global));
    register_id const result = allocate_register(&state->used_registers);
    read_structure_element_result const element_read =
        read_structure_element(state, to, state->global, global, name, where, result);
    if (!element_read.success)
    {
        restore(previous_code);
        return evaluate_expression_result_empty;
    }
    return evaluate_expression_result_create(
        evaluation_status_value, result, element_read.type_, element_read.compile_time_value, true);
}

static evaluate_expression_result make_unit(register_id *const used_registers, instruction_sequence *output)
{
    type const unit_type = {type_kind_unit, {0}};
    evaluate_expression_result const final_result =
        evaluate_expression_result_create(evaluation_status_value, allocate_register(used_registers), unit_type,
                                          optional_value_create(value_from_unit()), true);
    add_instruction(output, instruction_create_literal(
                                literal_instruction_create(final_result.where, value_from_unit(), type_from_unit())));
    return final_result;
}

static void define_register_debug_name(function_checking_state *const state, register_id const which,
                                       unicode_string const name)
{
    ASSUME(which < state->used_registers);
    register_id const previous_size = state->register_debug_name_count;
    if (which >= previous_size)
    {
        register_id const new_size = (which + 1);
        state->register_debug_names =
            reallocate_array(state->register_debug_names, new_size, sizeof(*state->register_debug_names));
        for (size_t i = previous_size; i < new_size; ++i)
        {
            state->register_debug_names[i] = unicode_string_from_c_str("");
        }
        state->register_debug_name_count = new_size;
        ASSUME(state->register_debug_name_count <= state->used_registers);
    }
    unicode_string_free(state->register_debug_names + which);
    state->register_debug_names[which] = name;
}

typedef struct conversion_result
{
    success_indicator ok;
    register_id where;
    type result_type;
    optional_value compile_time_value;
} conversion_result;

static conversion_result convert(function_checking_state *const state, instruction_sequence *const function,
                                 register_id const original, type const from,
                                 optional_value const original_compile_time_value,
                                 source_location const original_source, type const to,
                                 bool const may_widen_return_type);

static void check_function_clean_up(function_checking_state *const state)
{
    for (size_t i = 0; i < state->local_variables.count; ++i)
    {
        local_variable *const variable = (state->local_variables.elements + i);
        local_variable_free(variable);
    }
    if (state->local_variables.elements)
    {
        deallocate(state->local_variables.elements);
    }
    if (state->register_compile_time_values)
    {
        deallocate(state->register_compile_time_values);
        state->register_compile_time_values = NULL;
        state->register_compile_time_value_count = 0;
    }
}

static check_function_result finish_function_check(function_checking_state state, type const return_type,
                                                   instruction_sequence const body_out)
{
    type *const capture_types =
        state.capture_count ? allocate_array(state.capture_count, sizeof(*capture_types)) : NULL;
    for (size_t i = 0; i < state.capture_count; ++i)
    {
        capture_types[i] = state.captures[i].what;
    }

    function_pointer *const signature = allocate(sizeof(*signature));
    *signature =
        function_pointer_create(optional_type_create_set(return_type), tuple_type_create(NULL, 0),
                                tuple_type_create(capture_types, state.capture_count), optional_type_create_empty());

    ASSUME(state.register_debug_name_count <= state.used_registers);
    if (state.register_debug_name_count < state.used_registers)
    {
        state.register_debug_names =
            reallocate_array(state.register_debug_names, state.used_registers, sizeof(*state.register_debug_names));
        for (register_id i = state.register_debug_name_count; i < state.used_registers; ++i)
        {
            state.register_debug_names[i] = unicode_string_from_c_str("");
        }
    }
    checked_function const result =
        checked_function_create(signature, body_out, state.register_debug_names, state.used_registers);

    check_function_clean_up(&state);
    return check_function_result_create(result, state.captures, state.capture_count);
}

check_function_result
check_function(program_check *const root, function_checking_state *const parent, expression const body_in,
               structure const global, check_error_handler *const on_error, void *const user,
               checked_program *const program, type const *const parameter_types, unicode_string *const parameter_names,
               size_t const parameter_count, optional_type const self, bool const may_capture_runtime_variables,
               optional_type const explicit_return_type, source_file_owning const *const source,
               unicode_view const current_import_directory, unicode_view const *const early_initialized_variable,
               optional_function_id const current_function_id)
{
    ASSUME(root);
    instruction_sequence body_out = instruction_sequence_create(NULL, 0);
    function_checking_state state = function_checking_state_create(
        root, parent, may_capture_runtime_variables, &global, on_error, user, program, &body_out, explicit_return_type,
        explicit_return_type.is_set, source, current_import_directory);

    if (early_initialized_variable && current_function_id.is_set && explicit_return_type.is_set)
    {
        type const recursive_lambda_type = type_from_lambda(lambda_type_create(current_function_id.value));
        initialize_lambda_being_checked(
            &state.local_variables, *early_initialized_variable, recursive_lambda_type, &state);
    }

    if (self.is_set)
    {
        register_id const address = allocate_register(&state.used_registers);
        add_local_variable(
            &state.local_variables,
            local_variable_create(unicode_view_copy(unicode_view_from_c_str("self")), local_variable_phase_initialized,
                                  self.value, optional_value_empty, address, NULL));
        define_register_debug_name(&state, address, unicode_view_copy(unicode_view_from_c_str("self")));
    }

    for (size_t i = 0; i < parameter_count; ++i)
    {
        register_id const address = allocate_register(&state.used_registers);
        add_local_variable(&state.local_variables,
                           local_variable_create(unicode_view_copy(unicode_view_from_string(parameter_names[i])),
                                                 local_variable_phase_initialized, parameter_types[i],
                                                 optional_value_empty, address, NULL));
        define_register_debug_name(&state, address, unicode_view_copy(unicode_view_from_string(parameter_names[i])));
    }

    ASSUME(state.register_debug_name_count <= state.used_registers);

    evaluate_expression_result const body_evaluated =
        evaluate_expression(&state, &body_out, body_in, NULL, type_expectation_create_exact(explicit_return_type));

    ASSUME(state.register_debug_name_count <= state.used_registers);

    if (body_evaluated.status == evaluation_status_error)
    {
        for (size_t i = 0; i < state.register_debug_name_count; ++i)
        {
            unicode_string_free(state.register_debug_names + i);
        }
        if (state.register_debug_names)
        {
            deallocate(state.register_debug_names);
        }
        if (state.captures)
        {
            deallocate(state.captures);
        }
        instruction_sequence_free(&body_out);
        check_function_clean_up(&state);
        return check_function_result_empty;
    }

    bool const body_has_value = (body_evaluated.status == evaluation_status_value);
    bool const body_always_returns = (body_evaluated.status == evaluation_status_return);
    if (!body_has_value && !body_always_returns)
    {
        if (!explicit_return_type.is_set)
        {
            register_id const return_value_goes_into = allocate_register(&state.used_registers);
            add_instruction(&body_out, instruction_create_literal(literal_instruction_create(
                                           return_value_goes_into, value_from_unit(), type_from_unit())));
            register_id const unit_goes_into = allocate_register(&state.used_registers);
            add_instruction(&body_out, instruction_create_return(
                                           return_instruction_create(return_value_goes_into, unit_goes_into)));
        }
        ASSUME(state.register_debug_name_count <= state.used_registers);
        return finish_function_check(state, type_from_unit(), body_out);
    }

    if (explicit_return_type.is_set)
    {
        if (body_always_returns)
        {
            if (!state.return_type.is_set)
            {
                LPG_TO_DO();
            }
            return finish_function_check(state, state.return_type.value, body_out);
        }
        else
        {
            conversion_result const converted = convert(
                &state, &body_out, body_evaluated.where, body_evaluated.type_, body_evaluated.compile_time_value,
                expression_source_begin(body_in), explicit_return_type.value, false);
            switch (converted.ok)
            {
            case success_no:
                for (size_t i = 0; i < state.register_debug_name_count; ++i)
                {
                    unicode_string_free(state.register_debug_names + i);
                }
                if (state.register_debug_names)
                {
                    deallocate(state.register_debug_names);
                }
                instruction_sequence_free(&body_out);
                check_function_clean_up(&state);
                return check_function_result_empty;

            case success_yes:
                ASSUME(type_equals(explicit_return_type.value, converted.result_type));
                break;
            }
            register_id const unit_goes_into = allocate_register(&state.used_registers);
            add_instruction(
                &body_out, instruction_create_return(return_instruction_create(converted.where, unit_goes_into)));
            return finish_function_check(state, converted.result_type, body_out);
        }
    }
    else
    {
        if (body_always_returns)
        {
            if (!state.return_type.is_set)
            {
                LPG_TO_DO();
            }
            return finish_function_check(state, state.return_type.value, body_out);
        }
        else
        {
            if (body_has_value)
            {
                register_id const unit_goes_into = allocate_register(&state.used_registers);
                add_instruction(&body_out, instruction_create_return(
                                               return_instruction_create(body_evaluated.where, unit_goes_into)));
                return finish_function_check(state, body_evaluated.type_, body_out);
            }
            else
            {
                for (size_t i = 0; i < state.register_debug_name_count; ++i)
                {
                    unicode_string_free(state.register_debug_names + i);
                }
                if (state.register_debug_names)
                {
                    deallocate(state.register_debug_names);
                }
                instruction_sequence_free(&body_out);
                check_function_clean_up(&state);
                return check_function_result_empty;
            }
        }
    }
}

typedef struct compile_time_type_expression_result
{
    type compile_time_value;
    register_id where;
    bool has_value;
} compile_time_type_expression_result;

static compile_time_type_expression_result compile_time_type_expression_result_create(bool has_value, register_id where,
                                                                                      type compile_time_value)
{
    compile_time_type_expression_result const result = {compile_time_value, where, has_value};
    return result;
}

static compile_time_type_expression_result
expect_compile_time_type(function_checking_state *state, instruction_sequence *function, expression const element)
{
    evaluate_expression_result const result = evaluate_expression(
        state, function, element, NULL, type_expectation_create_exact(optional_type_create_empty()));
    switch (result.status)
    {
    case evaluation_status_value:
        if (!result.compile_time_value.is_set || (result.compile_time_value.value_.kind != value_kind_type))
        {
            emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                             expression_source_begin(element)));
            return compile_time_type_expression_result_create(false, ~(register_id)0, type_from_unit());
        }
        return compile_time_type_expression_result_create(true, result.where, result.compile_time_value.value_.type_);

    case evaluation_status_error:
    case evaluation_status_exit:
    case evaluation_status_return:
        return compile_time_type_expression_result_create(false, ~(register_id)0, type_from_unit());
    }
    LPG_UNREACHABLE();
}

typedef struct evaluated_function_header
{
    optional_type return_type;
    type *parameter_types;
    unicode_string *parameter_names;
    bool is_success;
} evaluated_function_header;

static evaluated_function_header
evaluated_function_header_create(type *parameter_types, unicode_string *parameter_names, optional_type return_type)
{
    evaluated_function_header const result = {return_type, parameter_types, parameter_names, true};
    return result;
}

static evaluated_function_header const evaluated_function_header_failure = {
    {false, {type_kind_unit, {0}}}, NULL, NULL, false};

static void evaluate_function_header_clean_up(type *const parameter_types, unicode_string *const parameter_names,
                                              size_t const parameter_count)
{
    for (size_t i = 0; i < parameter_count; ++i)
    {
        unicode_string_free(parameter_names + i);
    }
    deallocate(parameter_names);
    deallocate(parameter_types);
}

static evaluated_function_header evaluate_function_header(function_checking_state *const state,
                                                          instruction_sequence *const function,
                                                          function_header_tree const header)
{
    type *const parameter_types = allocate_array(header.parameter_count, sizeof(*parameter_types));
    unicode_string *const parameter_names = allocate_array(header.parameter_count, sizeof(*parameter_names));
    for (size_t i = 0; i < header.parameter_count; ++i)
    {
        instruction_checkpoint const before = make_checkpoint(state, function);
        parameter const this_parameter = header.parameters[i];
        compile_time_type_expression_result const parameter_type =
            expect_compile_time_type(state, function, *this_parameter.type);
        if (!parameter_type.has_value)
        {
            evaluate_function_header_clean_up(parameter_types, parameter_names, i);
            return evaluated_function_header_failure;
        }
        parameter_types[i] = parameter_type.compile_time_value;
        parameter_names[i] = unicode_view_copy(unicode_view_from_string(this_parameter.name.value));
        restore(before);
    }
    optional_type return_type = optional_type_create_empty();
    if (header.return_type)
    {
        instruction_checkpoint const original = make_checkpoint(state, function);
        compile_time_type_expression_result const return_type_result =
            expect_compile_time_type(state, function, *header.return_type);
        restore(original);
        if (!return_type_result.has_value)
        {
            evaluate_function_header_clean_up(parameter_types, parameter_names, header.parameter_count);
            return evaluated_function_header_failure;
        }
        return_type = optional_type_create_set(return_type_result.compile_time_value);
    }
    return evaluated_function_header_create(parameter_types, parameter_names, return_type);
}

static function_id reserve_function_id(function_checking_state *const state)
{
    function_id const this_lambda_id = state->program->function_count;
    ++(state->program->function_count);
    state->program->functions =
        reallocate_array(state->program->functions, state->program->function_count, sizeof(*state->program->functions));
    {
        function_pointer *const dummy_signature = allocate(sizeof(*dummy_signature));
        *dummy_signature =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        state->program->functions[this_lambda_id] =
            checked_function_create(dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    return this_lambda_id;
}

static void find_generic_closures_in_expression(generic_closures *const closures, function_checking_state *const state,
                                                expression const from);

static generic_closures find_generic_lambda_closures(function_checking_state *const state, lambda const definition)
{
    generic_closures result = {NULL, 0};
    find_generic_closures_in_expression(&result, state, expression_from_lambda(definition));
    return result;
}

static evaluate_expression_result
evaluate_generic_lambda_expression(function_checking_state *state, instruction_sequence *function, lambda const element,
                                   unicode_view const *const early_initialized_variable)
{
    program_check *const root = state->root;
    root->generic_lambdas =
        reallocate_array(root->generic_lambdas, root->generic_lambda_count + 1, sizeof(*root->generic_lambdas));
    generic_lambda_id const id = root->generic_lambda_count;
    root->generic_lambdas[id] =
        generic_lambda_create(lambda_clone(element), generic_closures_create(NULL, 0), state->source,
                              unicode_view_copy(state->current_import_directory));
    root->generic_lambda_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    if (early_initialized_variable)
    {
        initialize_early(&state->local_variables, *early_initialized_variable, type_from_generic_lambda(),
                         optional_value_create(value_from_generic_lambda(id)), into);
    }
    {
        generic_closures const closures = find_generic_lambda_closures(state, element);
        root->generic_lambdas[id].closures = closures;
    }
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  into, value_from_generic_lambda(id), type_from_generic_lambda())));
    write_register_compile_time_value(state, into, value_from_generic_lambda(id));
    return evaluate_expression_result_create(evaluation_status_value, into, type_from_generic_lambda(),
                                             optional_value_create(value_from_generic_lambda(id)), true);
}

static evaluate_expression_result evaluate_lambda(function_checking_state *const state,
                                                  instruction_sequence *const function, lambda const evaluated,
                                                  unicode_view const *const early_initialized_variable,
                                                  optional_function_id const predetermined_lambda_id)
{
    if (evaluated.generic_parameters.count > 0)
    {
        return evaluate_generic_lambda_expression(state, function, evaluated, early_initialized_variable);
    }
    evaluated_function_header const header = evaluate_function_header(state, function, evaluated.header);
    if (!header.is_success)
    {
        return evaluate_expression_result_empty;
    }

    function_id const this_lambda_id =
        (predetermined_lambda_id.is_set ? predetermined_lambda_id.value : reserve_function_id(state));
    state->program->functions[this_lambda_id].signature->parameters =
        tuple_type_create(header.parameter_types, evaluated.header.parameter_count);
    if (header.return_type.is_set)
    {
        state->program->functions[this_lambda_id].signature->result = header.return_type;
    }

    check_function_result const checked = check_function(
        state->root, state, *evaluated.result, *state->global, state->on_error, state->user, state->program,
        header.parameter_types, header.parameter_names, evaluated.header.parameter_count, optional_type_create_empty(),
        true, header.return_type, state->source, state->current_import_directory, early_initialized_variable,
        optional_function_id_create(this_lambda_id));
    for (size_t i = 0; i < evaluated.header.parameter_count; ++i)
    {
        unicode_string_free(header.parameter_names + i);
    }
    deallocate(header.parameter_names);
    if (!checked.success)
    {
        return evaluate_expression_result_empty;
    }

    if (checked.function.body.length == 1)
    {
        instruction const body = checked.function.body.elements[0];
        ASSUME((body.type == instruction_call) || (body.type == instruction_return) || (body.type == instruction_loop));
    }
    ASSUME(checked.function.signature->parameters.length == 0);
    checked.function.signature->parameters.elements = header.parameter_types;
    checked.function.signature->parameters.length = evaluated.header.parameter_count;

    state->program->functions[this_lambda_id].signature->parameters = tuple_type_create(NULL, 0);
    checked_function_free(&state->program->functions[this_lambda_id]);
    state->program->functions[this_lambda_id] = checked.function;
    register_id const destination = allocate_register(&state->used_registers);
    type const result_type = type_from_lambda(lambda_type_create(this_lambda_id));
    if (checked.captures)
    {
        register_id *const captures = allocate_array(checked.capture_count, sizeof(*captures));
        for (size_t i = 0; i < checked.capture_count; ++i)
        {
            capture const current_capture = checked.captures[i];
            if (current_capture.from.captured_in_current_lambda.has_value)
            {
                register_id const captures_register = allocate_register(&state->used_registers);
                add_instruction(function, instruction_create_get_captures(captures_register));
                register_id const transferred_capture = allocate_register(&state->used_registers);
                add_instruction(function, instruction_create_read_struct(read_struct_instruction_create(
                                              captures_register, current_capture.from.captured_in_current_lambda.value,
                                              transferred_capture)));
                captures[i] = transferred_capture;
            }
            else
            {
                captures[i] = current_capture.from.local_address;
            }
        }
        value *compile_time_captures = garbage_collector_allocate_array(
            &state->program->memory, checked.capture_count, sizeof(*compile_time_captures));
        for (size_t i = 0; i < checked.capture_count; ++i)
        {
            optional_value const captured = read_register_compile_time_value(state, captures[i]);
            if (captured.is_set)
            {
                compile_time_captures[i] = captured.value_;
            }
            else
            {
                compile_time_captures = NULL;
                break;
            }
        }
        deallocate(checked.captures);
        if (compile_time_captures)
        {
            deallocate(captures);
            value const compile_time_lambda = value_from_function_pointer(
                function_pointer_value_from_internal(this_lambda_id, compile_time_captures, checked.capture_count));
            add_instruction(function, instruction_create_literal(
                                          literal_instruction_create(destination, compile_time_lambda, result_type)));
            return evaluate_expression_result_create(
                evaluation_status_value, destination, result_type, optional_value_create(compile_time_lambda), true);
        }
        add_instruction(function, instruction_create_lambda_with_captures(lambda_with_captures_instruction_create(
                                      destination, this_lambda_id, captures, checked.capture_count)));
        return evaluate_expression_result_create(
            evaluation_status_value, destination, result_type, optional_value_empty, false);
    }
    value const result = value_from_function_pointer(function_pointer_value_from_internal(this_lambda_id, NULL, 0));
    add_instruction(function, instruction_create_literal(literal_instruction_create(destination, result, result_type)));
    write_register_compile_time_value(state, destination, result);
    return evaluate_expression_result_create(
        evaluation_status_value, destination, result_type, optional_value_create(result), false);
}

static optional_size find_implementation(lpg_interface const *const in, type const self)
{
    for (size_t i = 0; i < in->implementation_count; ++i)
    {
        if (type_equals(in->implementations[i].self, self))
        {
            return make_optional_size(i);
        }
    }
    return optional_size_empty;
}

static optional_size evaluate_impl_core(function_checking_state *state, instruction_sequence *const function,
                                        impl_expression_method const *const method_trees,
                                        size_t const defined_method_count, type const self,
                                        interface_id const target_interface, source_location const self_source,
                                        source_location const impl_begin);

static void use_generic_closures(function_checking_state *const state, generic_closures const closures)
{
    for (size_t i = 0; i < closures.count; ++i)
    {
        register_id const closure_register = allocate_register(&state->used_registers);
        add_local_variable(
            &state->local_variables,
            local_variable_create(unicode_view_copy(unicode_view_from_string(closures.elements[i].name)),
                                  local_variable_phase_initialized, closures.elements[i].what,
                                  optional_value_create(closures.elements[i].content), closure_register, NULL));
        write_register_compile_time_value(state, closure_register, closures.elements[i].content);
    }
}

static optional_size instantiate_generic_impl(function_checking_state *const state, interface_id const target_interface,
                                              impl_expression const tree, generic_closures const closures,
                                              type const *const argument_types, value *const arguments, type const self,
                                              source_file_owning const *const original_source,
                                              unicode_view const original_current_import_directory)
{
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state interface_checking = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored_instructions,
        optional_type_create_empty(), false, original_source, original_current_import_directory);
    for (size_t i = 0; i < tree.generic_parameters.count; ++i)
    {
        register_id const argument_register = allocate_register(&interface_checking.used_registers);
        add_local_variable(
            &interface_checking.local_variables,
            local_variable_create(unicode_view_copy(unicode_view_from_string(tree.generic_parameters.names[i])),
                                  local_variable_phase_initialized, argument_types[i],
                                  optional_value_create(arguments[i]), argument_register, NULL));
        write_register_compile_time_value(&interface_checking, argument_register, arguments[i]);
    }
    use_generic_closures(&interface_checking, closures);
    optional_size const evaluated =
        evaluate_impl_core(&interface_checking, &ignored_instructions, tree.methods, tree.method_count, self,
                           target_interface, expression_source_begin(*tree.self), tree.begin);
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(interface_checking.local_variables);
    if (interface_checking.register_compile_time_values)
    {
        deallocate(interface_checking.register_compile_time_values);
    }
    return evaluated;
}

typedef struct infer_generic_arguments_result
{
    bool can_be_inferred;
    type *argument_types;
    value *arguments;
} infer_generic_arguments_result;

static void infer_generic_arguments_result_free(infer_generic_arguments_result const freed)
{
    if (freed.argument_types)
    {
        deallocate(freed.argument_types);
        deallocate(freed.arguments);
    }
}

static infer_generic_arguments_result infer_generic_arguments(function_checking_state *const state,
                                                              instruction_sequence *const function,
                                                              generic_instantiation_expression const self_tree,
                                                              type const self, source_file_owning const *const source,
                                                              unicode_view const original_current_import_directory,
                                                              generic_closures const closures)
{
    instruction_sequence ignored = {NULL, 0};
    function_checking_state inference_state = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored,
        optional_type_create_empty(), false, source, original_current_import_directory);
    use_generic_closures(&inference_state, closures);
    evaluate_expression_result const generic_evaluated =
        evaluate_expression(&inference_state, function, *self_tree.generic, NULL,
                            type_expectation_create_exact(optional_type_create_empty()));
    instruction_sequence_free(&ignored);
    function_checking_state_free(inference_state);
    switch (generic_evaluated.status)
    {
    case evaluation_status_value:
        break;

    case evaluation_status_error:
    {
        infer_generic_arguments_result const result = {false, NULL, NULL};
        return result;
    }

    case evaluation_status_exit:
    case evaluation_status_return:
        LPG_TO_DO();
    }
    if (!generic_evaluated.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    switch (self.kind)
    {
    case type_kind_structure:
    {
        if (generic_evaluated.compile_time_value.value_.kind != value_kind_generic_struct)
        {
            infer_generic_arguments_result const result = {false, NULL, NULL};
            return result;
        }
        program_check *const root = state->root;
        for (size_t i = 0; i < root->struct_instantiation_count; ++i)
        {
            generic_struct_instantiation *const instantiation = root->struct_instantiations + i;
            if (self.structure_ != instantiation->instantiated)
            {
                continue;
            }
            value *const arguments = allocate_array(instantiation->argument_count, sizeof(*arguments));
            if (instantiation->argument_count)
            {
                memcpy(arguments, instantiation->arguments, sizeof(*arguments) * instantiation->argument_count);
            }
            type *const argument_types = allocate_array(instantiation->argument_count, sizeof(*argument_types));
            if (instantiation->argument_count)
            {
                memcpy(argument_types, instantiation->argument_types,
                       sizeof(*argument_types) * instantiation->argument_count);
            }
            infer_generic_arguments_result const result = {true, argument_types, arguments};
            return result;
        }
        break;
    }

    case type_kind_interface:
    {
        if (generic_evaluated.compile_time_value.value_.kind != value_kind_generic_interface)
        {
            infer_generic_arguments_result const result = {false, NULL, NULL};
            return result;
        }
        program_check *const root = state->root;
        for (size_t i = 0; i < root->interface_instantiation_count; ++i)
        {
            generic_interface_instantiation *const instantiation = root->interface_instantiations + i;
            if (self.interface_ != instantiation->instantiated)
            {
                continue;
            }
            value *const arguments = allocate_array(instantiation->argument_count, sizeof(*arguments));
            if (instantiation->argument_count)
            {
                memcpy(arguments, instantiation->arguments, sizeof(*arguments) * instantiation->argument_count);
            }
            type *const argument_types = allocate_array(instantiation->argument_count, sizeof(*argument_types));
            if (instantiation->argument_count)
            {
                memcpy(argument_types, instantiation->argument_types,
                       sizeof(*argument_types) * instantiation->argument_count);
            }
            infer_generic_arguments_result const result = {true, argument_types, arguments};
            return result;
        }
        break;
    }

    case type_kind_enumeration:
    {
        if (generic_evaluated.compile_time_value.value_.kind != value_kind_generic_enum)
        {
            infer_generic_arguments_result const result = {false, NULL, NULL};
            return result;
        }
        program_check *const root = state->root;
        for (size_t i = 0; i < root->enum_instantiation_count; ++i)
        {
            generic_enum_instantiation *const instantiation = root->enum_instantiations + i;
            if (self.enum_ != instantiation->instantiated)
            {
                continue;
            }
            value *const arguments = allocate_array(instantiation->argument_count, sizeof(*arguments));
            if (instantiation->argument_count)
            {
                memcpy(arguments, instantiation->arguments, sizeof(*arguments) * instantiation->argument_count);
            }
            type *const argument_types = allocate_array(instantiation->argument_count, sizeof(*argument_types));
            if (instantiation->argument_count)
            {
                memcpy(argument_types, instantiation->argument_types,
                       sizeof(*argument_types) * instantiation->argument_count);
            }
            infer_generic_arguments_result const result = {true, argument_types, arguments};
            return result;
        }
        break;
    }

    case type_kind_unit:
    case type_kind_type:
    case type_kind_lambda:
    case type_kind_enum_constructor:
    case type_kind_function_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
    case type_kind_generic_lambda:
    case type_kind_generic_struct:
    case type_kind_host_value:
    case type_kind_integer_range:
    case type_kind_method_pointer:
    case type_kind_string:
    case type_kind_tuple:
    {
        infer_generic_arguments_result const result = {false, NULL, NULL};
        return result;
    }
    }
    infer_generic_arguments_result const result = {false, NULL, NULL};
    return result;
}

static optional_size try_to_instantiate_generic_impl(function_checking_state *const state,
                                                     interface_id const target_interface, generic_interface_id const id,
                                                     type const self, type *const argument_types,
                                                     value *const arguments)
{
    generic_interface *const generic = state->root->generic_interfaces + id;
    for (size_t i = 0; i < generic->generic_impl_count; ++i)
    {
        generic_impl *const impl = generic->generic_impls + i;
        if (impl->self.is_regular)
        {
            if (type_equals(impl->self.regular, self))
            {
                return instantiate_generic_impl(state, target_interface, impl->tree, impl->closures, argument_types,
                                                arguments, self, impl->source,
                                                unicode_view_from_string(impl->current_import_directory));
            }
        }
        else
        {
            instruction_sequence ignored = instruction_sequence_create(NULL, 0);
            infer_generic_arguments_result const inferred =
                infer_generic_arguments(state, &ignored, impl->self.generic, self, impl->source,
                                        unicode_view_from_string(impl->current_import_directory), impl->closures);
            instruction_sequence_free(&ignored);
            if (!inferred.can_be_inferred)
            {
                infer_generic_arguments_result_free(inferred);
                continue;
            }
            optional_size const result = instantiate_generic_impl(
                state, target_interface, impl->tree, impl->closures, inferred.argument_types, inferred.arguments, self,
                impl->source, unicode_view_from_string(impl->current_import_directory));
            infer_generic_arguments_result_free(inferred);
            return result;
        }
    }
    return optional_size_empty;
}

static optional_size instantiate_generic_impl_for_regular_interface(function_checking_state *const state,
                                                                    interface_id const to,
                                                                    generic_impl_regular_interface *const impl,
                                                                    type *const argument_types, value *const arguments,
                                                                    type const self)
{
    for (size_t i = 0; i < impl->instantiation_count; ++i)
    {
        generic_impl_regular_interface_instantiation *const instantiation = impl->instantiations + i;
        if (type_equals(instantiation->self, self))
        {
            LPG_TO_DO();
        }
    }
    return instantiate_generic_impl(state, to, impl->tree, impl->closures, argument_types, arguments, self,
                                    impl->source, unicode_view_from_string(impl->current_import_directory));
}

static optional_size require_implementation(function_checking_state *const state, instruction_sequence *const function,
                                            interface_id const to, type const self)
{
    optional_size const already_exists = find_implementation(state->program->interfaces + to, self);
    if (already_exists.state == optional_set)
    {
        return already_exists;
    }
    program_check *const root = state->root;
    for (size_t i = 0; i < root->interface_instantiation_count; ++i)
    {
        generic_interface_instantiation *const instantiation = root->interface_instantiations + i;
        if (instantiation->instantiated == to)
        {
            return try_to_instantiate_generic_impl(
                state, to, instantiation->generic, self, instantiation->argument_types, instantiation->arguments);
        }
    }
    for (size_t i = 0; i < root->generic_impls_for_regular_interfaces_count; ++i)
    {
        generic_impl_regular_interface *const impl = root->generic_impls_for_regular_interfaces + i;
        if (impl->interface_ != to)
        {
            continue;
        }
        infer_generic_arguments_result const inferred =
            infer_generic_arguments(state, function, impl->self, self, impl->source,
                                    unicode_view_from_string(impl->current_import_directory), impl->closures);
        if (!inferred.can_be_inferred)
        {
            continue;
        }
        optional_size const result = instantiate_generic_impl_for_regular_interface(
            state, to, impl, inferred.argument_types, inferred.arguments, self);
        deallocate(inferred.argument_types);
        deallocate(inferred.arguments);
        return result;
    }
    return optional_size_empty;
}

static conversion_result convert_to_interface(function_checking_state *const state,
                                              instruction_sequence *const function, register_id const original,
                                              type const from, source_location const original_source,
                                              interface_id const to)
{
    if (!state->root->interfaces_defined[to])
    {
        LPG_TO_DO();
    }
    optional_size const impl = require_implementation(state, function, to, from);
    if (impl.state == optional_empty)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch, original_source));
        conversion_result const result = {success_no, original, from, optional_value_empty};
        return result;
    }

    register_id const converted = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_erase_type(erase_type_instruction_create(
                                  original, converted, implementation_ref_create(to, impl.value_if_set))));
    conversion_result const result = {success_yes, converted, type_from_interface(to), optional_value_empty};
    return result;
}

static conversion_result convert(function_checking_state *const state, instruction_sequence *const function,
                                 register_id const original, type const from,
                                 optional_value const original_compile_time_value,
                                 source_location const original_source, type const to, bool const may_widen_result_type)
{
    if (type_equals(from, to))
    {
        conversion_result const result = {success_yes, original, to, original_compile_time_value};
        return result;
    }
    switch (to.kind)
    {
    case type_kind_generic_struct:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
    case type_kind_generic_lambda:
    case type_kind_enum_constructor:
    case type_kind_enumeration:
    case type_kind_function_pointer:
    case type_kind_method_pointer:
    case type_kind_lambda:
    case type_kind_string:
    case type_kind_unit:
    case type_kind_structure:
    case type_kind_type:
    case type_kind_host_value:
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch, original_source));
        conversion_result const result = {success_no, original, from, optional_value_empty};
        return result;
    }

    case type_kind_tuple:
        LPG_TO_DO();

    case type_kind_integer_range:
    {
        if (from.kind != type_kind_integer_range)
        {
            emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch, original_source));
            conversion_result const result = {success_no, original, from, optional_value_empty};
            return result;
        }
        if (integer_less_or_equals(to.integer_range_.minimum, from.integer_range_.minimum) &&
            integer_less_or_equals(from.integer_range_.maximum, to.integer_range_.maximum))
        {
            conversion_result const result = {success_yes, original, to, original_compile_time_value};
            return result;
        }
        if (may_widen_result_type)
        {
            conversion_result const result = {success_yes, original, type_from_integer_range(integer_range_combine(
                                                                         from.integer_range_, to.integer_range_)),
                                              original_compile_time_value};
            return result;
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch, original_source));
        conversion_result const result = {success_no, original, from, optional_value_empty};
        return result;
    }

    case type_kind_interface:
        return convert_to_interface(state, function, original, from, original_source, to.interface_);
    }
    LPG_UNREACHABLE();
}

typedef struct argument_evaluation_result
{
    optional_value compile_time_value;
    register_id where;
    success_indicator ok;
} argument_evaluation_result;

static argument_evaluation_result evaluate_argument(function_checking_state *const state,
                                                    instruction_sequence *const function,
                                                    expression const argument_tree, type const callee_type,
                                                    size_t const parameter_id)
{
    type const parameter_type =
        get_parameter_type(callee_type, parameter_id, state->program->functions, state->program->enums);
    evaluate_expression_result const argument = evaluate_expression(
        state, function, argument_tree, NULL, type_expectation_create_exact(optional_type_create_set(parameter_type)));
    switch (argument.status)
    {
    case evaluation_status_error:
    case evaluation_status_exit:
    case evaluation_status_return:
    {
        argument_evaluation_result const result = {optional_value_empty, 0, success_no};
        return result;
    }

    case evaluation_status_value:
        break;
    }
    conversion_result const converted =
        convert(state, function, argument.where, argument.type_, argument.compile_time_value,
                expression_source_begin(argument_tree), parameter_type, false);
    argument_evaluation_result const result = {converted.compile_time_value, converted.where, converted.ok};
    return result;
}

static evaluate_expression_result evaluate_call_expression(function_checking_state *state,
                                                           instruction_sequence *function, call const called,
                                                           type_expectation const expected_result_type)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, function);
    type_expectation expected_call_type = type_expectation_create_exact(optional_type_create_empty());
    if (expected_result_type.exact_type.is_set && (expected_result_type.exact_type.value.kind == type_kind_enumeration))
    {
        expected_call_type = type_expectation_create_enum_constructors(expected_result_type.exact_type.value.enum_);
    }
    evaluate_expression_result const callee =
        evaluate_expression(state, function, *called.callee, NULL, expected_call_type);
    switch (callee.status)
    {
    case evaluation_status_error:
    case evaluation_status_exit:
    case evaluation_status_return:
        ASSUME(state->register_debug_name_count <= state->used_registers);
        return evaluate_expression_result_empty;

    case evaluation_status_value:
        break;
    }

    switch (callee.type_.kind)
    {
    case type_kind_lambda:
    case type_kind_function_pointer:
    case type_kind_enum_constructor:
        break;

    case type_kind_generic_struct:
    case type_kind_host_value:
    case type_kind_generic_lambda:
    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        ASSUME(state->register_debug_name_count <= state->used_registers);
        emit_semantic_error(
            state, semantic_error_create(semantic_error_not_callable, expression_source_begin(*called.callee)));
        return evaluate_expression_result_empty;
    }
    size_t const expected_arguments =
        expected_call_argument_count(callee.type_, state->program->functions, state->program->enums);
    register_id *const arguments = allocate_array(expected_arguments, sizeof(*arguments));
    value *const compile_time_arguments = allocate_array(expected_arguments, sizeof(*compile_time_arguments));
    bool all_compile_time_arguments = true;
    LPG_FOR(size_t, i, called.arguments.length)
    {
        expression const argument_tree = called.arguments.elements[i];
        if (i >= expected_arguments)
        {
            emit_semantic_error(state, semantic_error_create(
                                           semantic_error_extraneous_argument, expression_source_begin(argument_tree)));
            break;
        }
        argument_evaluation_result const result = evaluate_argument(state, function, argument_tree, callee.type_, i);
        if (result.ok != success_yes)
        {
            ASSUME(state->register_debug_name_count <= state->used_registers);
            restore(previous_code);
            deallocate(compile_time_arguments);
            deallocate(arguments);
            ASSUME(state->register_debug_name_count <= state->used_registers);
            return evaluate_expression_result_empty;
        }
        if (result.compile_time_value.is_set)
        {
            compile_time_arguments[i] = result.compile_time_value.value_;
        }
        else
        {
            all_compile_time_arguments = false;
        }
        arguments[i] = result.where;
    }
    if (called.arguments.length < expected_arguments)
    {
        ASSUME(state->register_debug_name_count <= state->used_registers);
        deallocate(compile_time_arguments);
        restore(previous_code);
        deallocate(arguments);
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_argument, called.closing_parenthesis));
        ASSUME(state->register_debug_name_count <= state->used_registers);
        return evaluate_expression_result_empty;
    }
    optional_type const return_type =
        get_return_type(callee.type_, state->program->functions, state->program->interfaces);
    register_id result = ~(register_id)0;
    optional_value compile_time_result = {false, value_from_unit()};
    if (callee.compile_time_value.is_set && all_compile_time_arguments)
    {
        {
            switch (callee.compile_time_value.value_.kind)
            {
            case value_kind_generic_struct:
            case value_kind_generic_lambda:
            case value_kind_array:
                LPG_TO_DO();

            case value_kind_function_pointer:
            {
                if (!callee.compile_time_value.value_.function_pointer.external &&
                    (state->program->functions[callee.compile_time_value.value_.function_pointer.code].body.length ==
                     0))
                {
                    // This function has no body which means it is currently being type
                    // checked and can't be called yet.
                    break;
                }
                external_function_result const call_result =
                    call_function(callee.compile_time_value.value_.function_pointer, optional_value_empty,
                                  compile_time_arguments, &state->root->compile_time_interpreter);
                switch (call_result.code)
                {
                case external_function_result_out_of_memory:
                    emit_semantic_error(state, semantic_error_create(semantic_error_compile_time_memory_limit_reached,
                                                                     called.arguments.opening_brace));
                    ASSUME(!compile_time_result.is_set);
                    break;

                case external_function_result_success:
                    compile_time_result = optional_value_create(call_result.if_success);
                    break;

                case external_function_result_unavailable:
                    ASSUME(!compile_time_result.is_set);
                    break;

                case external_function_result_stack_overflow:
                    emit_semantic_error(
                        state, semantic_error_create(semantic_error_stack_overflow, called.arguments.opening_brace));
                    ASSUME(!compile_time_result.is_set);
                    break;

                case external_function_result_instruction_limit_reached:
                    emit_semantic_error(state, semantic_error_create(semantic_error_instruction_limit_reached,
                                                                     called.arguments.opening_brace));
                    ASSUME(!compile_time_result.is_set);
                    break;
                }
                break;
            }

            case value_kind_type_erased:
            case value_kind_integer:
            case value_kind_string:
            case value_kind_structure:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
            case value_kind_pattern:
            case value_kind_generic_enum:
            case value_kind_generic_interface:
                LPG_UNREACHABLE();

            case value_kind_enum_constructor:
            {
                value *const enum_state = garbage_collector_allocate(&state->program->memory, sizeof(*enum_state));
                ASSUME(expected_arguments == 1);
                *enum_state = compile_time_arguments[0];
                optional_type const enum_state_type = state->program->enums[callee.type_.enum_constructor->enumeration]
                                                          .elements[callee.type_.enum_constructor->which]
                                                          .state;
                ASSUME(enum_state_type.is_set);
                compile_time_result = optional_value_create(
                    value_from_enum_element(callee.type_.enum_constructor->which, enum_state_type.value, enum_state));
                break;
            }
            }
        }
        if (compile_time_result.is_set)
        {
            if (value_is_mutable(compile_time_result.value_))
            {
                compile_time_result = optional_value_empty;
            }
            else
            {
                deallocate(arguments);
                restore(previous_code);
                result = allocate_register(&state->used_registers);
                if (!return_type.is_set)
                {
                    LPG_TO_DO();
                }
                if (return_type.value.kind == type_kind_string)
                {
                    size_t const length = compile_time_result.value_.string.length;
                    char *const copy = garbage_collector_allocate(&state->program->memory, length);
                    memcpy(copy, compile_time_result.value_.string.begin, length);
                    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                  result, value_from_string(unicode_view_create(copy, length)),
                                                  type_from_string())));
                }
                else
                {
                    if (!return_type.is_set)
                    {
                        LPG_TO_DO();
                    }
                    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                  result, compile_time_result.value_, return_type.value)));
                }
            }
        }
    }

    if (!compile_time_result.is_set)
    {
        result = allocate_register(&state->used_registers);
        switch (callee.type_.kind)
        {
        case type_kind_generic_struct:
        case type_kind_host_value:
        case type_kind_generic_lambda:
        case type_kind_method_pointer:
        case type_kind_interface:
        case type_kind_generic_interface:
            LPG_TO_DO();

        case type_kind_lambda:
        case type_kind_function_pointer:
            add_instruction(function, instruction_create_call(call_instruction_create(
                                          callee.where, arguments, expected_arguments, result)));
            break;

        case type_kind_structure:
        case type_kind_unit:
        case type_kind_string:
        case type_kind_enumeration:
        case type_kind_tuple:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_generic_enum:
            LPG_UNREACHABLE();

        case type_kind_enum_constructor:
            ASSUME(called.arguments.length == 1);
            optional_type const enum_state = state->program->enums[callee.type_.enum_constructor->enumeration]
                                                 .elements[callee.type_.enum_constructor->which]
                                                 .state;
            ASSUME(enum_state.is_set);
            add_instruction(function, instruction_create_enum_construct(enum_construct_instruction_create(
                                          result, *callee.type_.enum_constructor, arguments[0], enum_state.value)));
            deallocate(arguments);
            break;
        }
    }

    ASSUME(state->register_debug_name_count <= state->used_registers);

    deallocate(compile_time_arguments);
    if (!return_type.is_set)
    {
        ASSUME(state->register_debug_name_count <= state->used_registers);
        return evaluate_expression_result_create(
            evaluation_status_exit, result, type_from_unit(), compile_time_result, false);
    }
    ASSUME(state->register_debug_name_count <= state->used_registers);
    return evaluate_expression_result_create(
        evaluation_status_value, result, return_type.value, compile_time_result, false);
}

evaluate_expression_result evaluate_not_expression(function_checking_state *state, instruction_sequence *function,
                                                   const expression *element)
{
    value const boolean_value = state->global->members[3].compile_time_value.value_;
    ASSUME(boolean_value.kind == value_kind_type);

    evaluate_expression_result const result =
        evaluate_expression(state, function, *(*element).not.expr, NULL,
                            type_expectation_create_exact(optional_type_create_set(boolean_value.type_)));
    ASSUME(state->global->members[3].compile_time_value.is_set);

    if (type_equals(boolean_value.type_, result.type_))
    {
        const register_id global_register = allocate_register(&state->used_registers);
        add_instruction(function, instruction_create_global(global_register));
        const register_id not_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_read_struct(read_struct_instruction_create(global_register, 7, not_register)));
        register_id *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = result.where;
        const register_id result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_call(call_instruction_create(not_register, arguments, 1, result_register)));
        return evaluate_expression_result_create(
            evaluation_status_value, result_register, result.type_, optional_value_empty, false);
    }
    emit_semantic_error(
        state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin((*element))));
    return evaluate_expression_result_empty;
}

static void deallocate_boolean_cases(match_instruction_case *cases, bool *cases_handled, size_t array_size)
{
    for (size_t j = 0; j < array_size; ++j)
    {
        match_instruction_case_free(cases[j]);
    }
    if (cases)
    {
        deallocate(cases);
    }
    deallocate(cases_handled);
}

static void deallocate_integer_range_list_cases(match_instruction_case *cases, size_t const case_count)
{
    for (size_t j = 0; j < case_count; ++j)
    {
        match_instruction_case_free(cases[j]);
    }
    if (cases)
    {
        deallocate(cases);
    }
}

static void deallocate_cases(match_instruction_case *cases, size_t const case_count)
{
    for (size_t j = 0; j < case_count; ++j)
    {
        match_instruction_case_free(cases[j]);
    }
    if (cases)
    {
        deallocate(cases);
    }
}

typedef enum pattern_evaluate_result_kind {
    pattern_evaluate_result_kind_is_pattern = 1,
    pattern_evaluate_result_kind_no_pattern,
    pattern_evaluate_result_kind_failure
} pattern_evaluate_result_kind;

typedef struct pattern_evaluate_result
{
    enum_constructor_type stateful_enum_element;
    unicode_view placeholder_name;
    source_location placeholder_name_source;
    pattern_evaluate_result_kind kind;
} pattern_evaluate_result;

static pattern_evaluate_result const pattern_evaluate_result_no = {
    {~(enum_id)0, ~(enum_element_id)0}, {NULL, 0}, {0, 0}, pattern_evaluate_result_kind_no_pattern};

static pattern_evaluate_result const pattern_evaluate_result_failure = {
    {~(enum_id)0, ~(enum_element_id)0}, {NULL, 0}, {0, 0}, pattern_evaluate_result_kind_failure};

static pattern_evaluate_result pattern_evaluate_result_create_pattern(enum_constructor_type stateful_enum_element,
                                                                      unicode_view placeholder_name,
                                                                      source_location placeholder_name_source)
{
    pattern_evaluate_result const result = {
        stateful_enum_element, placeholder_name, placeholder_name_source, pattern_evaluate_result_kind_is_pattern};
    return result;
}

static pattern_evaluate_result check_for_pattern(function_checking_state *state, instruction_sequence *function,
                                                 expression const root, enum_id const expected_enumeration)
{
    switch (root.type)
    {
    case expression_type_new_array:
    case expression_type_import:
    case expression_type_generic_instantiation:
        LPG_TO_DO();

    case expression_type_call:
    {
        if ((root.call.arguments.length != 1) || (root.call.arguments.elements[0].type != expression_type_placeholder))
        {
            return pattern_evaluate_result_no;
        }
        instruction_checkpoint const before_pattern = make_checkpoint(state, function);
        evaluate_expression_result const enum_element_evaluated = evaluate_expression(
            state, function, *root.call.callee, NULL, type_expectation_create_enum_constructors(expected_enumeration));
        restore(before_pattern);
        switch (enum_element_evaluated.status)
        {
        case evaluation_status_error:
        case evaluation_status_exit:
        case evaluation_status_return:
            return pattern_evaluate_result_failure;

        case evaluation_status_value:
            break;
        }
        if (enum_element_evaluated.type_.kind != type_kind_enum_constructor)
        {
            return pattern_evaluate_result_no;
        }
        return pattern_evaluate_result_create_pattern(
            *enum_element_evaluated.type_.enum_constructor,
            unicode_view_from_string(root.call.arguments.elements[0].placeholder.name),
            root.call.arguments.elements[0].placeholder.where);
    }

    case expression_type_lambda:
    case expression_type_integer_literal:
    case expression_type_access_structure:
    case expression_type_match:
    case expression_type_string:
    case expression_type_identifier:
    case expression_type_not:
    case expression_type_binary:
    case expression_type_return:
    case expression_type_loop:
    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_comment:
    case expression_type_interface:
    case expression_type_struct:
    case expression_type_impl:
    case expression_type_instantiate_struct:
    case expression_type_enum:
    case expression_type_placeholder:
    case expression_type_type_of:
        return pattern_evaluate_result_no;
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result check_sequence_finish(function_checking_state *const state,
                                                        instruction_sequence *const output, sequence const input,
                                                        size_t const previous_number_of_variables,
                                                        type_expectation const expected_result_type)
{
    if (input.length == 0)
    {
        return make_unit(&state->used_registers, output);
    }
    bool is_pure = true;
    bool always_returns = false;
    evaluate_expression_result final_result = evaluate_expression_result_empty;
    LPG_FOR(size_t, i, input.length)
    {
        bool const is_last_in_sequence = (i == (input.length - 1));
        final_result = evaluate_expression(
            state, output, input.elements[i], NULL,
            is_last_in_sequence ? expected_result_type : type_expectation_create_exact(optional_type_create_empty()));
        if (!final_result.is_pure)
        {
            is_pure = false;
        }
        if (final_result.status == evaluation_status_return)
        {
            always_returns = true;
        }
    }
    for (size_t i = previous_number_of_variables, c = state->local_variables.count; i != c; ++i)
    {
        local_variable_free(state->local_variables.elements + i);
    }
    state->local_variables.count = previous_number_of_variables;
    return evaluate_expression_result_create(always_returns ? evaluation_status_return : final_result.status,
                                             final_result.where, final_result.type_, final_result.compile_time_value,
                                             is_pure);
}

static bool merge_types(type *result_type, const type evaluated_type)
{
    if (type_equals(*result_type, evaluated_type))
    {
        return true;
    }

    if (result_type->kind != evaluated_type.kind || result_type->kind != type_kind_integer_range)
    {
        return false;
    }

    *result_type =
        type_from_integer_range(integer_range_combine(result_type->integer_range_, evaluated_type.integer_range_));

    return true;
}

static bool check_for_duplicate_string_case(function_checking_state *const state, match_instruction_case *const cases,
                                            size_t const existing_case_count, unicode_view const new_case_value)
{
    for (size_t i = 0; i < existing_case_count; ++i)
    {
        switch (cases[i].kind)
        {
        case match_instruction_case_kind_default:
            break;

        case match_instruction_case_kind_stateful_enum:
            LPG_UNREACHABLE();

        case match_instruction_case_kind_value:
        {
            optional_value const key = state->register_compile_time_values[cases[i].key_value];
            ASSUME(key.is_set);
            ASSUME(key.value_.kind == value_kind_string);
            if (unicode_view_equals(key.value_.string, new_case_value))
            {
                return true;
            }
            break;
        }
        }
    }
    return false;
}

static evaluate_expression_result evaluate_match_expression_with_string(function_checking_state *const state,
                                                                        instruction_sequence *const function,
                                                                        expression const *const element,
                                                                        evaluate_expression_result const input,
                                                                        type_expectation const expected_result_type)
{
    ASSUME(input.type_.kind == type_kind_string);
    instruction_checkpoint const before = make_checkpoint(state, function);
    type result_type = type_from_unit();
    bool all_cases_return = true;
    match_instruction_case *const cases = allocate_array((*element).match.number_of_cases, sizeof(*cases));
    optional_size default_index = optional_size_empty;
    optional_value compile_time_result = optional_value_empty;
    optional_value default_compile_time_result = optional_value_empty;
    bool can_have_compile_time_result = true;
    for (size_t i = 0; i < (*element).match.number_of_cases; ++i)
    {
        match_case const case_tree = (*element).match.cases[i];
        expression *const key = case_tree.key_or_default;
        evaluate_expression_result key_evaluated
#ifdef _MSC_VER
            // avoid false positive warning about uninitialized variable
            = evaluate_expression_result_empty
#endif
            ;
        if (key)
        {
            key_evaluated =
                evaluate_expression(state, function, *key, NULL,
                                    type_expectation_create_exact(optional_type_create_set(type_from_string())));
            if (key_evaluated.status != evaluation_status_value)
            {
                deallocate_cases(cases, i);
                return key_evaluated;
            }
            if (key_evaluated.type_.kind != type_kind_string)
            {
                emit_semantic_error(
                    state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*key)));
                deallocate_cases(cases, i);
                return evaluate_expression_result_empty;
            }
            if (!key_evaluated.compile_time_value.is_set)
            {
                LPG_TO_DO();
            }
            ASSUME(key_evaluated.compile_time_value.value_.kind == value_kind_string);
            if (check_for_duplicate_string_case(
                    state, cases, (*element).match.number_of_cases, key_evaluated.compile_time_value.value_.string))
            {
                emit_semantic_error(
                    state, semantic_error_create(semantic_error_duplicate_match_case, expression_source_begin(*key)));
                deallocate_cases(cases, i);
                return evaluate_expression_result_empty;
            }
        }
        else
        {
            if (default_index.state == optional_set)
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_duplicate_default_case,
                                                                 expression_source_begin(*case_tree.action)));
                deallocate_cases(cases, i);
                return evaluate_expression_result_empty;
            }
            default_index = make_optional_size(i);
        }

        instruction_sequence action = instruction_sequence_create(NULL, 0);
        evaluate_expression_result const action_evaluated =
            evaluate_expression(state, &action, *case_tree.action, NULL, expected_result_type);
        if (action_evaluated.status != evaluation_status_value)
        {
            instruction_sequence_free(&action);
            deallocate_cases(cases, i);
            return action_evaluated;
        }

        all_cases_return = (all_cases_return && (action_evaluated.status == evaluation_status_return));

        if (i == 0)
        {
            result_type = action_evaluated.type_;
        }
        else if (!type_equals(result_type, action_evaluated.type_))
        {
            /*TODO: support types that are not the same, but still comparable*/
            emit_semantic_error(
                state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.action)));
            instruction_sequence_free(&action);
            deallocate_cases(cases, i);
            return evaluate_expression_result_empty;
        }

        can_have_compile_time_result =
            (can_have_compile_time_result && input.compile_time_value.is_set && input.is_pure &&
             action_evaluated.compile_time_value.is_set && action_evaluated.is_pure);
        if (key)
        {
            if (can_have_compile_time_result &&
                value_equals(input.compile_time_value.value_, key_evaluated.compile_time_value.value_))
            {
                ASSUME(!compile_time_result.is_set);
                compile_time_result = optional_value_create(action_evaluated.compile_time_value.value_);
            }
            cases[i] = match_instruction_case_create_value(
                key_evaluated.where, action, optional_register_id_create_set(action_evaluated.where));
        }
        else
        {
            if (can_have_compile_time_result)
            {
                default_compile_time_result = action_evaluated.compile_time_value;
            }
            cases[i] =
                match_instruction_case_create_default(action, optional_register_id_create_set(action_evaluated.where));
        }
    }

    if (default_index.state == optional_empty)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_missing_default, expression_source_begin(*element)));
        deallocate_cases(cases, (*element).match.number_of_cases);
        return evaluate_expression_result_empty;
    }

    if (can_have_compile_time_result && !compile_time_result.is_set)
    {
        compile_time_result = default_compile_time_result;
    }

    if (compile_time_result.is_set)
    {
        deallocate_cases(cases, (*element).match.number_of_cases);
        restore(before);
        register_id const result_register = allocate_register(&state->used_registers);
        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      result_register, compile_time_result.value_, result_type)));
        return evaluate_expression_result_create(all_cases_return ? evaluation_status_return : evaluation_status_value,
                                                 result_register, result_type, compile_time_result, true);
    }

    /* The default case is always the last case in order to save work in the backends. */
    ASSUME(default_index.state == optional_set);
    match_instruction_case_swap((cases + default_index.value_if_set), (cases + (*element).match.number_of_cases - 1));

    register_id const result_register = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_match(match_instruction_create(
                                  input.where, cases, (*element).match.number_of_cases, result_register, result_type)));
    return evaluate_expression_result_create(all_cases_return ? evaluation_status_return : evaluation_status_value,
                                             result_register, result_type, optional_value_empty, false);
}

evaluate_expression_result evaluate_match_expression(function_checking_state *state, instruction_sequence *function,
                                                     const expression *element,
                                                     type_expectation const expected_result_type)
{
    instruction_checkpoint const before = make_checkpoint(state, function);
    evaluate_expression_result const input = evaluate_expression(
        state, function, *(*element).match.input, NULL, type_expectation_create_exact(optional_type_create_empty()));
    if (input.status != evaluation_status_value)
    {
        return input;
    }
    switch (input.type_.kind)
    {
    case type_kind_generic_struct:
    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_enumeration:
    {
        enumeration const *const enum_ = state->program->enums + input.type_.enum_;
        if (enum_->size == 0)
        {
            emit_semantic_error(
                state, semantic_error_create(semantic_error_match_unsupported, expression_source_begin((*element))));
            return evaluate_expression_result_empty;
        }

        if (enum_->size != (*element).match.number_of_cases)
        {
            emit_semantic_error(
                state, semantic_error_create(semantic_error_missing_match_case, expression_source_begin((*element))));
            return evaluate_expression_result_empty;
        }

        match_instruction_case *const cases = allocate_array((*element).match.number_of_cases, sizeof(*cases));
        bool *const enum_elements_handled =
            allocate_array((*element).match.number_of_cases, sizeof(*enum_elements_handled));
        memset(enum_elements_handled, 0, (*element).match.number_of_cases * sizeof(*enum_elements_handled));
        type result_type = type_from_unit();
        optional_value compile_time_result = optional_value_empty;
        bool all_cases_return = true;
        for (size_t i = 0; i < (*element).match.number_of_cases; ++i)
        {
            match_case const case_tree = (*element).match.cases[i];
            bool is_always_this_case = false;
            register_id key_value = ~(register_id)0;
            instruction_sequence action = instruction_sequence_create(NULL, 0);
            evaluate_expression_result action_evaluated;
            register_id placeholder_where = ~(register_id)0;
            if (!case_tree.key_or_default)
            {
                LPG_TO_DO();
            }
            expression *const key = case_tree.key_or_default;
            pattern_evaluate_result const maybe_pattern = check_for_pattern(state, function, *key, input.type_.enum_);
            switch (maybe_pattern.kind)
            {
            case pattern_evaluate_result_kind_is_pattern:
            {
                if (maybe_pattern.stateful_enum_element.enumeration != input.type_.enum_)
                {
                    emit_semantic_error(
                        state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*key)));
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return evaluate_expression_result_empty;
                }

                {
                    bool *const case_handled = (enum_elements_handled + maybe_pattern.stateful_enum_element.which);
                    if (*case_handled)
                    {
                        emit_semantic_error(state, semantic_error_create(semantic_error_duplicate_match_case,
                                                                         expression_source_begin(*key)));
                        deallocate_boolean_cases(cases, enum_elements_handled, i);
                        return evaluate_expression_result_empty;
                    }
                    *case_handled = true;
                }

                size_t const previous_number_of_variables = state->local_variables.count;

                placeholder_where = allocate_register(&state->used_registers);
                if (local_variable_name_exists(state->local_variables, maybe_pattern.placeholder_name))
                {
                    emit_semantic_error(state, semantic_error_create(semantic_error_declaration_with_existing_name,
                                                                     maybe_pattern.placeholder_name_source));
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return evaluate_expression_result_empty;
                }

                ASSUME(placeholder_where != ~(register_id)0);
                ASSUME(enum_->elements[maybe_pattern.stateful_enum_element.which].state.is_set);
                add_local_variable(
                    &state->local_variables,
                    local_variable_create(unicode_view_copy(maybe_pattern.placeholder_name),
                                          local_variable_phase_initialized,
                                          enum_->elements[maybe_pattern.stateful_enum_element.which].state.value,
                                          optional_value_empty, placeholder_where, NULL));
                define_register_debug_name(state, placeholder_where, unicode_view_copy(maybe_pattern.placeholder_name));

                action_evaluated = check_sequence_finish(
                    state, &action, sequence_create(case_tree.action, 1, expression_source_begin(*case_tree.action)),
                    previous_number_of_variables, expected_result_type);
                break;
            }

            case pattern_evaluate_result_kind_no_pattern:
            {
                evaluate_expression_result const key_evaluated = evaluate_expression(
                    state, function, *key, NULL, type_expectation_create_exact(optional_type_create_set(input.type_)));
                if (key_evaluated.status != evaluation_status_value)
                {
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return key_evaluated;
                }

                if (!type_equals(input.type_, key_evaluated.type_))
                {
                    emit_semantic_error(
                        state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*key)));
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return evaluate_expression_result_empty;
                }

                if (key_evaluated.compile_time_value.value_.enum_element.state_type.kind != type_kind_unit)
                {
                    /* https://github.com/TyRoXx/Lpg/issues/91 */
                    LPG_TO_DO();
                }

                {
                    ASSUME(key_evaluated.compile_time_value.value_.kind == value_kind_enum_element);
                    bool *const case_handled =
                        (enum_elements_handled + key_evaluated.compile_time_value.value_.enum_element.which);
                    if (*case_handled)
                    {
                        emit_semantic_error(state, semantic_error_create(semantic_error_duplicate_match_case,
                                                                         expression_source_begin(*key)));
                        deallocate_boolean_cases(cases, enum_elements_handled, i);
                        return evaluate_expression_result_empty;
                    }
                    *case_handled = true;
                }

                /*TODO: support runtime values as keys?*/
                ASSERT(key_evaluated.compile_time_value.is_set);

                is_always_this_case =
                    input.compile_time_value.is_set && input.is_pure &&
                    value_equals(input.compile_time_value.value_, key_evaluated.compile_time_value.value_);
                key_value = key_evaluated.where;

                action_evaluated = evaluate_expression(state, &action, *case_tree.action, NULL, expected_result_type);
                break;
            }

            case pattern_evaluate_result_kind_failure:
                deallocate_boolean_cases(cases, enum_elements_handled, i);
                return evaluate_expression_result_empty;

#ifdef _MSC_VER
            default:
                // silence MSVC warning about uninitialized variables
                LPG_UNREACHABLE();
#endif
            }

            if (action_evaluated.status == evaluation_status_error)
            {
                instruction_sequence_free(&action);
                deallocate_boolean_cases(cases, enum_elements_handled, i);
                return evaluate_expression_result_empty;
            }

            if (action_evaluated.status == evaluation_status_value)
            {
                if (i == 0)
                {
                    result_type = action_evaluated.type_;
                }
                else if (!merge_types(&result_type, action_evaluated.type_))
                {
                    /*TODO: support types that are not the same, but still comparable*/
                    emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch,
                                                                     expression_source_begin(*case_tree.action)));
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    instruction_sequence_free(&action);
                    return evaluate_expression_result_empty;
                }
            }

            if (!compile_time_result.is_set && is_always_this_case && action_evaluated.compile_time_value.is_set &&
                action_evaluated.is_pure)
            {
                compile_time_result = optional_value_create(action_evaluated.compile_time_value.value_);
            }

            all_cases_return = (all_cases_return && ((action_evaluated.status == evaluation_status_return) ||
                                                     (action_evaluated.status == evaluation_status_exit)));

            switch (maybe_pattern.kind)
            {
            case pattern_evaluate_result_kind_is_pattern:
                ASSUME(placeholder_where != ~(register_id)0);
                cases[i] = match_instruction_case_create_stateful_enum(
                    match_instruction_case_stateful_enum_create(
                        maybe_pattern.stateful_enum_element.which, placeholder_where),
                    action, (action_evaluated.status == evaluation_status_value)
                                ? optional_register_id_create_set(action_evaluated.where)
                                : optional_register_id_create_empty());
                break;

            case pattern_evaluate_result_kind_no_pattern:
                ASSUME(key_value != ~(register_id)0);
                cases[i] = match_instruction_case_create_value(
                    key_value, action, (action_evaluated.status == evaluation_status_value)
                                           ? optional_register_id_create_set(action_evaluated.where)
                                           : optional_register_id_create_empty());
                break;

            case pattern_evaluate_result_kind_failure:
                LPG_UNREACHABLE();
            }
        }
        for (size_t i = 0; i < (*element).match.number_of_cases; ++i)
        {
            ASSUME(enum_elements_handled[i]);
            switch (cases[i].kind)
            {
            case match_instruction_case_kind_default:
                LPG_TO_DO();

            case match_instruction_case_kind_stateful_enum:
                break;

            case match_instruction_case_kind_value:
                for (size_t k = (i + 1); k < (*element).match.number_of_cases; ++k)
                {
                    switch (cases[k].kind)
                    {
                    case match_instruction_case_kind_stateful_enum:
                        break;

                    case match_instruction_case_kind_value:
                    {
                        optional_value const left = read_register_compile_time_value(state, cases[i].key_value);
                        optional_value const right = read_register_compile_time_value(state, cases[k].key_value);
                        if (left.is_set && right.is_set)
                        {
                            ASSUME(!value_equals(left.value_, right.value_));
                        }
                        break;
                    }

                    case match_instruction_case_kind_default:
                        LPG_TO_DO();
                    }
                }
                break;
            }
        }
        register_id result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_match(match_instruction_create(
                          input.where, cases, (*element).match.number_of_cases, result_register, result_type)));
        deallocate(enum_elements_handled);
        if (compile_time_result.is_set)
        {
            restore(before);
            result_register = allocate_register(&state->used_registers);
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          result_register, compile_time_result.value_, result_type)));
            return evaluate_expression_result_create(
                all_cases_return ? evaluation_status_return : evaluation_status_value, result_register, result_type,
                compile_time_result, true);
        }

        return evaluate_expression_result_create(all_cases_return ? evaluation_status_return : evaluation_status_value,
                                                 result_register, result_type, optional_value_empty, false);
    }

    case type_kind_integer_range:
    {
        integer const expected_cases = integer_range_size(input.type_.integer_range_);

        if (!integer_equal(expected_cases, integer_create(0, (*element).match.number_of_cases)))
        {
            emit_semantic_error(
                state, semantic_error_create(semantic_error_missing_match_case, expression_source_begin((*element))));
            return evaluate_expression_result_empty;
        }

        match_instruction_case *const cases = allocate_array((*element).match.number_of_cases, sizeof(*cases));

        integer_set integer_ranges_unhandled = integer_set_from_range(input.type_.integer_range_);

        type result_type = type_from_unit();
        optional_value compile_time_result = optional_value_empty;

        bool all_cases_return = true;
        size_t const number_of_cases = (*element).match.number_of_cases;
        for (size_t i = 0; i < number_of_cases; ++i)
        {
            match_case const case_tree = (*element).match.cases[i];
            if (!case_tree.key_or_default)
            {
                LPG_TO_DO();
            }
            expression *const key = case_tree.key_or_default;
            evaluate_expression_result const key_evaluated = evaluate_expression(
                state, function, *key, NULL, type_expectation_create_exact(optional_type_create_set(input.type_)));
            if (key_evaluated.status != evaluation_status_value)
            {
                deallocate_integer_range_list_cases(cases, i);
                integer_set_free(integer_ranges_unhandled);
                return key_evaluated;
            }

            if (key_evaluated.type_.kind != type_kind_integer_range)
            {
                emit_semantic_error(
                    state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*key)));
                deallocate_integer_range_list_cases(cases, i);
                integer_set_free(integer_ranges_unhandled);
                return evaluate_expression_result_empty;
            }

            {
                if (integer_set_contains_range(integer_ranges_unhandled, key_evaluated.type_.integer_range_))
                {
                    integer_set_remove_range(&integer_ranges_unhandled, key_evaluated.type_.integer_range_);
                }
                else
                {
                    // TODO: Better error description
                    emit_semantic_error(state, semantic_error_create(
                                                   semantic_error_duplicate_match_case, expression_source_begin(*key)));
                    deallocate_integer_range_list_cases(cases, i);
                    integer_set_free(integer_ranges_unhandled);
                    return evaluate_expression_result_empty;
                }
            }

            /*TODO: support runtime values as keys?*/
            ASSERT(key_evaluated.compile_time_value.is_set);

            instruction_sequence action = instruction_sequence_create(NULL, 0);
            evaluate_expression_result const action_evaluated =
                evaluate_expression(state, &action, *case_tree.action, NULL, expected_result_type);
            switch (action_evaluated.status)
            {
            case evaluation_status_value:
            case evaluation_status_return:
            case evaluation_status_exit:
                break;

            case evaluation_status_error:
                deallocate_integer_range_list_cases(cases, i);
                integer_set_free(integer_ranges_unhandled);
                instruction_sequence_free(&action);
                return action_evaluated;
            }

            all_cases_return = (all_cases_return && ((action_evaluated.status == evaluation_status_return) ||
                                                     (action_evaluated.status == evaluation_status_exit)));

            if (action_evaluated.status == evaluation_status_value)
            {
                if (i == 0)
                {
                    result_type = action_evaluated.type_;
                }
                else if (!type_equals(result_type, action_evaluated.type_))
                {
                    /*TODO: support types that are not the same, but still comparable*/
                    emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch,
                                                                     expression_source_begin(*case_tree.action)));
                    deallocate_integer_range_list_cases(cases, i);
                    integer_set_free(integer_ranges_unhandled);
                    instruction_sequence_free(&action);
                    return evaluate_expression_result_empty;
                }
            }

            if (!compile_time_result.is_set && input.compile_time_value.is_set && input.is_pure &&
                value_equals(input.compile_time_value.value_, key_evaluated.compile_time_value.value_) &&
                action_evaluated.compile_time_value.is_set && action_evaluated.is_pure)
            {
                compile_time_result = optional_value_create(action_evaluated.compile_time_value.value_);
            }

            cases[i] = match_instruction_case_create_value(
                key_evaluated.where, action, (action_evaluated.status == evaluation_status_value)
                                                 ? optional_register_id_create_set(action_evaluated.where)
                                                 : optional_register_id_create_empty());
        }

        ASSUME(integer_set_is_empty(integer_ranges_unhandled));
        if (compile_time_result.is_set)
        {
            deallocate_integer_range_list_cases(cases, (*element).match.number_of_cases);
            integer_set_free(integer_ranges_unhandled);
            restore(before);
            register_id const result_register = allocate_register(&state->used_registers);
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          result_register, compile_time_result.value_, result_type)));
            return evaluate_expression_result_create(
                all_cases_return ? evaluation_status_return : evaluation_status_value, result_register, result_type,
                compile_time_result, true);
        }
        integer_set_free(integer_ranges_unhandled);
        register_id const result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_match(match_instruction_create(
                          input.where, cases, (*element).match.number_of_cases, result_register, result_type)));
        return evaluate_expression_result_create(all_cases_return ? evaluation_status_return : evaluation_status_value,
                                                 result_register, result_type, optional_value_empty, false);
    }

    case type_kind_string:
        return evaluate_match_expression_with_string(state, function, element, input, expected_result_type);

    case type_kind_unit:
    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_lambda:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_interface:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        emit_semantic_error(
            state, semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*element)));
        return evaluate_expression_result_empty;
    }
    LPG_UNREACHABLE();
}

typedef struct evaluate_struct_arguments_result
{
    register_id *registers;
} evaluate_struct_arguments_result;

static evaluate_struct_arguments_result evaluate_struct_arguments(function_checking_state *const state,
                                                                  instruction_sequence *const function,
                                                                  expression const *const arguments,
                                                                  size_t const argument_count,
                                                                  structure_member const *const members)
{
    register_id *registers = allocate_array(argument_count, sizeof(*registers));
    for (size_t i = 0; i < argument_count; ++i)
    {
        type const member_type = members[i].what;
        evaluate_expression_result const argument_evaluated = evaluate_expression(
            state, function, arguments[i], NULL, type_expectation_create_exact(optional_type_create_set(member_type)));
        if (argument_evaluated.status != evaluation_status_value)
        {
            deallocate(registers);
            evaluate_struct_arguments_result const result = {NULL};
            return result;
        }
        if (type_equals(argument_evaluated.type_, member_type))
        {
            registers[i] = argument_evaluated.where;
        }
        else
        {
            conversion_result const converted = convert(state, function, argument_evaluated.where,
                                                        argument_evaluated.type_, argument_evaluated.compile_time_value,
                                                        expression_source_begin(arguments[i]), member_type, false);
            switch (converted.ok)
            {
            case success_no:
            {
                deallocate(registers);
                evaluate_struct_arguments_result const result = {NULL};
                return result;
            }

            case success_yes:
                break;
            }
            registers[i] = converted.where;
        }
    }
    evaluate_struct_arguments_result const result = {registers};
    return result;
}

static void find_generic_closures_in_function_header(generic_closures *const closures,
                                                     function_checking_state *const state,
                                                     function_header_tree const header)
{
    for (size_t i = 0; i < header.parameter_count; ++i)
    {
        find_generic_closures_in_expression(closures, state, *header.parameters[i].type);
    }
    if (header.return_type)
    {
        find_generic_closures_in_expression(closures, state, *header.return_type);
    }
}

static void find_generic_closures_in_sequence(generic_closures *const closures, function_checking_state *const state,
                                              sequence const tree)
{
    for (size_t i = 0; i < tree.length; ++i)
    {
        find_generic_closures_in_expression(closures, state, tree.elements[i]);
    }
}

static void resolve_generic_closure_identifier(generic_closures *const closures, function_checking_state *const state,
                                               unicode_view const name, source_location const source)
{
    for (size_t i = 0; i < state->local_variables.count; ++i)
    {
        if (unicode_view_equals(name, unicode_view_from_string(state->local_variables.elements[i].name)))
        {
            if (!state->local_variables.elements[i].compile_time_value.is_set)
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type, source));
                return;
            }
            closures->elements = reallocate_array(closures->elements, closures->count + 1, sizeof(*closures->elements));
            closures->elements[closures->count] =
                generic_closure_create(unicode_view_copy(name), state->local_variables.elements[i].type_,
                                       state->local_variables.elements[i].compile_time_value.value_);
            closures->count += 1;
            return;
        }
    }
    if (state->parent)
    {
        resolve_generic_closure_identifier(closures, state->parent, name, source);
    }
}

static void find_generic_closures_in_expression(generic_closures *const closures, function_checking_state *const state,
                                                expression const from)
{
    switch (from.type)
    {
    case expression_type_new_array:
        find_generic_closures_in_expression(closures, state, *from.new_array.element);
        break;

    case expression_type_type_of:
        find_generic_closures_in_expression(closures, state, *from.type_of.target);
        break;

    case expression_type_import:
    case expression_type_binary:
        LPG_TO_DO();

    case expression_type_lambda:
        find_generic_closures_in_function_header(closures, state, from.lambda.header);
        find_generic_closures_in_expression(closures, state, *from.lambda.result);
        break;

    case expression_type_call:
        find_generic_closures_in_expression(closures, state, *from.call.callee);
        for (size_t i = 0; i < from.call.arguments.length; ++i)
        {
            find_generic_closures_in_expression(closures, state, from.call.arguments.elements[i]);
        }
        break;

    case expression_type_access_structure:
        find_generic_closures_in_expression(closures, state, *from.access_structure.object);
        break;

    case expression_type_match:
        find_generic_closures_in_expression(closures, state, *from.match.input);
        for (size_t i = 0; i < from.match.number_of_cases; ++i)
        {
            if (from.match.cases[i].key_or_default)
            {
                find_generic_closures_in_expression(closures, state, *from.match.cases[i].key_or_default);
            }
            find_generic_closures_in_expression(closures, state, *from.match.cases[i].action);
        }
        break;

    case expression_type_integer_literal:
    case expression_type_string:
    case expression_type_break:
    case expression_type_comment:
    case expression_type_placeholder:
        break;

    case expression_type_identifier:
        resolve_generic_closure_identifier(
            closures, state, unicode_view_from_string(from.identifier.value), from.identifier.source);
        break;

    case expression_type_not:
        find_generic_closures_in_expression(closures, state, *from.not.expr);
        break;

    case expression_type_return:
        find_generic_closures_in_expression(closures, state, *from.return_);
        break;

    case expression_type_loop:
        for (size_t i = 0; i < from.loop_body.length; ++i)
        {
            find_generic_closures_in_expression(closures, state, from.loop_body.elements[i]);
        }
        break;

    case expression_type_sequence:
        find_generic_closures_in_sequence(closures, state, from.sequence);
        break;

    case expression_type_declare:
        if (from.declare.optional_type)
        {
            find_generic_closures_in_expression(closures, state, *from.declare.optional_type);
        }
        find_generic_closures_in_expression(closures, state, *from.declare.initializer);
        break;

    case expression_type_interface:
        for (size_t i = 0; i < from.interface_.method_count; ++i)
        {
            find_generic_closures_in_function_header(closures, state, from.interface_.methods[i].header);
        }
        break;

    case expression_type_struct:
        for (size_t i = 0; i < from.struct_.element_count; ++i)
        {
            find_generic_closures_in_expression(closures, state, from.struct_.elements[i].type);
        }
        break;

    case expression_type_impl:
        find_generic_closures_in_expression(closures, state, *from.impl.interface_);
        find_generic_closures_in_expression(closures, state, *from.impl.self);
        for (size_t i = 0; i < from.impl.method_count; ++i)
        {
            find_generic_closures_in_function_header(closures, state, from.impl.methods[i].header);
            find_generic_closures_in_sequence(closures, state, from.impl.methods[i].body);
        }
        break;

    case expression_type_instantiate_struct:
        find_generic_closures_in_expression(closures, state, *from.instantiate_struct.type);
        for (size_t i = 0; i < from.instantiate_struct.arguments.length; ++i)
        {
            find_generic_closures_in_expression(closures, state, from.instantiate_struct.arguments.elements[i]);
        }
        break;

    case expression_type_enum:
        for (size_t i = 0; i < from.enum_.element_count; ++i)
        {
            if (from.enum_.elements[i].state)
            {
                find_generic_closures_in_expression(closures, state, *from.enum_.elements[i].state);
            }
        }
        break;

    case expression_type_generic_instantiation:
        find_generic_closures_in_expression(closures, state, *from.generic_instantiation.generic);
        for (size_t i = 0; i < from.generic_instantiation.count; ++i)
        {
            find_generic_closures_in_expression(closures, state, from.generic_instantiation.arguments[i]);
        }
        break;
    }
}

static generic_closures find_generic_closures_in_impl(function_checking_state *const state, impl_expression const from)
{
    generic_closures result = {NULL, 0};
    find_generic_closures_in_expression(&result, state, *from.interface_);
    find_generic_closures_in_expression(&result, state, *from.self);
    for (size_t i = 0; i < from.method_count; ++i)
    {
        impl_expression_method const method = from.methods[i];
        find_generic_closures_in_function_header(&result, state, method.header);
        find_generic_closures_in_sequence(&result, state, method.body);
    }
    return result;
}

static generic_closures find_generic_interface_closures(function_checking_state *const state,
                                                        interface_expression const definition)
{
    generic_closures result = {NULL, 0};
    for (size_t i = 0; i < definition.method_count; ++i)
    {
        interface_expression_method const method = definition.methods[i];
        find_generic_closures_in_function_header(&result, state, method.header);
    }
    return result;
}

static generic_closures find_generic_struct_closures(function_checking_state *const state,
                                                     struct_expression const definition)
{
    generic_closures result = {NULL, 0};
    for (size_t i = 0; i < definition.element_count; ++i)
    {
        struct_expression_element const element = definition.elements[i];
        find_generic_closures_in_expression(&result, state, element.type);
    }
    return result;
}

static evaluate_expression_result
evaluate_generic_interface_expression(function_checking_state *state, instruction_sequence *function,
                                      interface_expression const element,
                                      unicode_view const *const early_initialized_variable)
{
    program_check *const root = state->root;
    root->generic_interfaces = reallocate_array(
        root->generic_interfaces, root->generic_interface_count + 1, sizeof(*root->generic_interfaces));
    generic_interface_id const id = root->generic_interface_count;
    register_id const into = allocate_register(&state->used_registers);

    if (early_initialized_variable)
    {
        initialize_early(&state->local_variables, *early_initialized_variable, type_from_generic_interface(),
                         optional_value_create(value_from_generic_interface(id)), into);
    }

    generic_closures const closures = find_generic_interface_closures(state, element);
    root->generic_interfaces[id] = generic_interface_create(
        interface_expression_clone(element), closures, unicode_view_copy(state->current_import_directory));
    root->generic_interface_count += 1;

    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  into, value_from_generic_interface(id), type_from_generic_interface())));
    write_register_compile_time_value(state, into, value_from_generic_interface(id));
    return evaluate_expression_result_create(evaluation_status_value, into, type_from_generic_interface(),
                                             optional_value_create(value_from_generic_interface(id)), true);
}

static evaluate_expression_result evaluate_interface(function_checking_state *state, instruction_sequence *function,
                                                     interface_expression const element,
                                                     unicode_view const *const early_initialized_variable)
{
    if (element.parameters.count > 0)
    {
        return evaluate_generic_interface_expression(state, function, element, early_initialized_variable);
    }

    interface_id const id = state->program->interface_count;
    state->root->interfaces_defined =
        reallocate_array(state->root->interfaces_defined, (id + 1), sizeof(*state->root->interfaces_defined));
    state->root->interfaces_defined[id] = false;
    state->program->interfaces =
        reallocate_array(state->program->interfaces, (id + 1), sizeof(*state->program->interfaces));
    state->program->interfaces[id] = interface_create(NULL, 0, NULL, 0);
    state->program->interface_count += 1;

    register_id const into = allocate_register(&state->used_registers);

    if (early_initialized_variable)
    {
        initialize_early(&state->local_variables, *early_initialized_variable, type_from_type(),
                         optional_value_create(value_from_type(type_from_interface(id))), into);
    }

    method_description *const checked_methods = allocate_array(element.method_count, sizeof(*checked_methods));
    function_id checked_method_count = 0;
    for (size_t i = 0; i < element.method_count; ++i)
    {
        interface_expression_method const method = element.methods[i];
        evaluated_function_header header = evaluate_function_header(state, function, method.header);
        if (!header.is_success)
        {
            continue;
        }
        if (!header.return_type.is_set)
        {
            header.return_type = optional_type_create_set(type_from_unit());
        }
        for (size_t j = 0; j < method.header.parameter_count; ++j)
        {
            unicode_string_free(header.parameter_names + j);
        }
        if (header.parameter_names)
        {
            deallocate(header.parameter_names);
        }
        for (size_t k = 0; k < checked_method_count; ++k)
        {
            if (unicode_view_equals(
                    unicode_view_from_string(checked_methods[k].name), unicode_view_from_string(method.name.value)))
            {
                emit_semantic_error(
                    state, semantic_error_create(semantic_error_duplicate_method_name, method.name.source));
            }
        }
        checked_methods[checked_method_count] = method_description_create(
            unicode_view_copy(unicode_view_from_string(method.name.value)),
            tuple_type_create(header.parameter_types, method.header.parameter_count), header.return_type.value);
        checked_method_count += 1;
    }

    state->root->interfaces_defined[id] = true;
    state->program->interfaces[id].methods = checked_methods;
    state->program->interfaces[id].method_count = checked_method_count;

    value const result = value_from_type(type_from_interface(id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, result, type_from_type())));
    return evaluate_expression_result_create(
        evaluation_status_value, into, type_from_type(), optional_value_create(result), false);
}

static evaluate_expression_result
evaluate_generic_struct_expression(function_checking_state *state, instruction_sequence *function,
                                   struct_expression const element,
                                   unicode_view const *const early_initialized_variable)
{
    program_check *const root = state->root;
    root->generic_structs =
        reallocate_array(root->generic_structs, root->generic_struct_count + 1, sizeof(*root->generic_structs));
    generic_struct_id const id = root->generic_struct_count;
    register_id const into = allocate_register(&state->used_registers);

    if (early_initialized_variable)
    {
        initialize_early(&state->local_variables, *early_initialized_variable, type_from_generic_struct(),
                         optional_value_create(value_from_generic_struct(id)), into);
    }

    generic_closures const closures = find_generic_struct_closures(state, element);
    root->generic_structs[id] = generic_struct_create(
        struct_expression_clone(element), closures, unicode_view_copy(state->current_import_directory));
    root->generic_struct_count += 1;

    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  into, value_from_generic_struct(id), type_from_generic_struct())));
    write_register_compile_time_value(state, into, value_from_generic_struct(id));
    return evaluate_expression_result_create(evaluation_status_value, into, type_from_generic_struct(),
                                             optional_value_create(value_from_generic_struct(id)), true);
}

static evaluate_expression_result evaluate_struct(function_checking_state *state, instruction_sequence *function,
                                                  struct_expression const evaluated,
                                                  unicode_view const *const early_initialized_variable)
{
    if (evaluated.generic_parameters.count > 0)
    {
        return evaluate_generic_struct_expression(state, function, evaluated, early_initialized_variable);
    }

    structure_member *elements = allocate_array(evaluated.element_count, sizeof(*elements));
    for (size_t i = 0; i < evaluated.element_count; ++i)
    {
        struct_expression_element const element = evaluated.elements[i];
        evaluate_expression_result const element_type_evaluated =
            evaluate_expression(state, function, element.type, NULL,
                                type_expectation_create_exact(optional_type_create_set(type_from_type())));
        if (element_type_evaluated.status != evaluation_status_value)
        {
            for (size_t j = 0; j < i; ++j)
            {
                struct_member_free(elements + j);
            }
            deallocate(elements);
            return evaluate_expression_result_empty;
        }
        if (!element_type_evaluated.compile_time_value.is_set ||
            (element_type_evaluated.compile_time_value.value_.kind != value_kind_type))
        {
            emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                             expression_source_begin(element.type)));
            for (size_t j = 0; j < i; ++j)
            {
                struct_member_free(elements + j);
            }
            deallocate(elements);
            return evaluate_expression_result_empty;
        }
        elements[i] = structure_member_create(element_type_evaluated.compile_time_value.value_.type_,
                                              unicode_view_copy(unicode_view_from_string(element.name.value)),
                                              optional_value_empty);
    }
    struct_id const id = state->program->struct_count;
    state->program->structs = reallocate_array(state->program->structs, (id + 1), sizeof(*state->program->structs));
    state->program->structs[id] = structure_create(elements,
                                                   /*TODO avoid truncation safely*/ (struct_id)evaluated.element_count);
    state->program->struct_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    value const result = value_from_type(type_from_struct(id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, result, type_from_type())));
    return evaluate_expression_result_create(
        evaluation_status_value, into, type_from_type(), optional_value_create(result), false);
}

typedef struct method_evaluation_result
{
    function_pointer_value success;
    bool is_success;
} method_evaluation_result;

static method_evaluation_result make_method_evaluation_result_failure(void)
{
    method_evaluation_result result;
    result.is_success = false;
    return result;
}

static method_evaluation_result method_evaluation_result_create(function_pointer_value success)
{
    method_evaluation_result const result = {
        success, true,
    };
    return result;
}

static method_evaluation_result evaluate_method_definition(function_checking_state *state,
                                                           instruction_sequence *const function,
                                                           impl_expression_method const method, type const self,
                                                           type const declared_result_type)
{
    evaluated_function_header const header = evaluate_function_header(state, function, method.header);
    if (!header.is_success)
    {
        return make_method_evaluation_result_failure();
    }
    if (header.return_type.is_set && !is_implicitly_convertible(header.return_type.value, declared_result_type))
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_type_mismatch,
                                                         expression_source_begin(*method.header.return_type)));
        for (size_t i = 0; i < method.header.parameter_count; ++i)
        {
            unicode_string_free(header.parameter_names + i);
        }
        deallocate(header.parameter_names);
        deallocate(header.parameter_types);
        return make_method_evaluation_result_failure();
    }
    function_id const this_lambda_id = reserve_function_id(state);
    check_function_result const checked = check_function(
        state->root, state, expression_from_sequence(method.body), *state->global, state->on_error, state->user,
        state->program, header.parameter_types, header.parameter_names, method.header.parameter_count,
        optional_type_create_set(self), false, optional_type_create_set(declared_result_type), state->source,
        state->current_import_directory, NULL, optional_function_id_empty());
    for (size_t i = 0; i < method.header.parameter_count; ++i)
    {
        unicode_string_free(header.parameter_names + i);
    }
    deallocate(header.parameter_names);
    if (!checked.success)
    {
        deallocate(header.parameter_types);
        return make_method_evaluation_result_failure();
    }
    ASSUME(!checked.captures);
    ASSUME(checked.function.signature->parameters.length == 0);
    checked.function.signature->parameters.elements = header.parameter_types;
    checked.function.signature->parameters.length = method.header.parameter_count;
    checked.function.signature->self = optional_type_create_set(self);

    checked_function_free(&state->program->functions[this_lambda_id]);
    state->program->functions[this_lambda_id] = checked.function;
    return method_evaluation_result_create(function_pointer_value_from_internal(this_lambda_id, NULL, 0));
}

static size_t begin_implementation(lpg_interface *const to, type const self)
{
    size_t const new_impl_id = to->implementation_count;
    size_t const new_impl_count = to->implementation_count + 1;
    to->implementations = reallocate_array(to->implementations, new_impl_count, sizeof(*to->implementations));
    to->implementations[to->implementation_count] = implementation_entry_create(self, implementation_create(NULL, 0));
    to->implementation_count = new_impl_count;
    return new_impl_id;
}

static void finish_implementation(lpg_interface *const to, size_t const impl_index, implementation const methods)
{
    ASSUME(impl_index < to->implementation_count);
    ASSUME(to->implementations[impl_index].target.method_count == 0);
    to->implementations[impl_index].target = methods;
}

static evaluate_expression_result evaluate_generic_impl_regular_self(function_checking_state *state,
                                                                     instruction_sequence *const function,
                                                                     impl_expression const element)
{
    generic_instantiation_expression const interface_instantiation = element.interface_->generic_instantiation;
    if (interface_instantiation.count != element.generic_parameters.count)
    {
        LPG_TO_DO();
    }
    for (size_t i = 0; i < interface_instantiation.count; ++i)
    {
        expression const argument = interface_instantiation.arguments[i];
        if (argument.type != expression_type_identifier)
        {
            LPG_TO_DO();
        }
        if (!unicode_view_equals(unicode_view_from_string(argument.identifier.value),
                                 unicode_view_from_string(element.generic_parameters.names[i])))
        {
            emit_semantic_error(state, semantic_error_create(
                                           semantic_error_generic_impl_parameter_mismatch, argument.identifier.source));
            return evaluate_expression_result_empty;
        }
    }

    evaluate_expression_result const generic =
        evaluate_expression(state, function, *interface_instantiation.generic, NULL,
                            type_expectation_create_exact(optional_type_create_empty()));
    switch (generic.status)
    {
    case evaluation_status_value:
        break;

    case evaluation_status_error:
        return evaluate_expression_result_empty;

    case evaluation_status_exit:
    case evaluation_status_return:
        LPG_TO_DO();
    }
    if (!generic.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    if (generic.compile_time_value.value_.kind != value_kind_generic_interface)
    {
        LPG_TO_DO();
    }

    evaluate_expression_result const self =
        evaluate_expression(state, function, *element.self, NULL,
                            type_expectation_create_exact(optional_type_create_set(type_from_type())));
    if (self.status != evaluation_status_value)
    {
        return evaluate_expression_result_empty;
    }
    if (!self.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    if (self.compile_time_value.value_.kind != value_kind_type)
    {
        LPG_TO_DO();
    }

    generic_closures const closures = find_generic_closures_in_impl(state, element);

    program_check *const root = state->root;
    generic_interface *const interface_ =
        &root->generic_interfaces[generic.compile_time_value.value_.generic_interface];
    interface_->generic_impls = reallocate_array(
        interface_->generic_impls, (interface_->generic_impl_count + 1), sizeof(*interface_->generic_impls));
    interface_->generic_impls[interface_->generic_impl_count] =
        generic_impl_create(impl_expression_clone(element), closures,
                            generic_impl_self_create_regular(self.compile_time_value.value_.type_), state->source,
                            unicode_view_copy(state->current_import_directory));
    interface_->generic_impl_count += 1;
    return make_unit(&state->used_registers, function);
}

static evaluate_expression_result evaluate_generic_impl_regular_interface(function_checking_state *state,
                                                                          instruction_sequence *const function,
                                                                          expression const interface_,
                                                                          generic_instantiation_expression const self,
                                                                          impl_expression const tree)
{
    evaluate_expression_result const interface_evaluated = evaluate_expression(
        state, function, interface_, NULL, type_expectation_create_exact(optional_type_create_set(type_from_type())));
    switch (interface_evaluated.status)
    {
    case evaluation_status_value:
        break;

    case evaluation_status_error:
        return evaluate_expression_result_empty;

    case evaluation_status_exit:
    case evaluation_status_return:
        LPG_TO_DO();
    }
    if (!interface_evaluated.compile_time_value.is_set)
    {
        emit_semantic_error(state, semantic_error_create(
                                       semantic_error_expected_compile_time_type, expression_source_begin(interface_)));
        return evaluate_expression_result_empty;
    }
    if (interface_evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        emit_semantic_error(state, semantic_error_create(
                                       semantic_error_expected_compile_time_type, expression_source_begin(interface_)));
        return evaluate_expression_result_empty;
    }
    if (interface_evaluated.compile_time_value.value_.type_.kind != type_kind_interface)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_expected_interface, expression_source_begin(interface_)));
        return evaluate_expression_result_empty;
    }

    generic_closures const closures = find_generic_closures_in_impl(state, tree);

    program_check *const root = state->root;
    root->generic_impls_for_regular_interfaces = reallocate_array(root->generic_impls_for_regular_interfaces,
                                                                  root->generic_impls_for_regular_interfaces_count + 1,
                                                                  sizeof(*root->generic_impls_for_regular_interfaces));
    root->generic_impls_for_regular_interfaces[root->generic_impls_for_regular_interfaces_count] =
        generic_impl_regular_interface_create(interface_evaluated.compile_time_value.value_.type_.interface_,
                                              impl_expression_clone(tree), closures,
                                              generic_instantiation_expression_clone(self), state->source,
                                              unicode_view_copy(state->current_import_directory));
    root->generic_impls_for_regular_interfaces_count += 1;
    return make_unit(&state->used_registers, function);
}

static evaluate_expression_result
evaluate_fully_generic_impl(function_checking_state *state, instruction_sequence *const function,
                            generic_instantiation_expression const interface_expression_,
                            generic_instantiation_expression const self, impl_expression const tree)
{
    if (interface_expression_.count != tree.generic_parameters.count)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_generic_impl_parameter_mismatch,
                                                         expression_source_begin(*interface_expression_.generic)));
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < interface_expression_.count; ++i)
    {
        expression const argument = interface_expression_.arguments[i];
        if (argument.type != expression_type_identifier)
        {
            LPG_TO_DO();
        }
        if (!unicode_view_equals(unicode_view_from_string(argument.identifier.value),
                                 unicode_view_from_string(tree.generic_parameters.names[i])))
        {
            emit_semantic_error(state, semantic_error_create(
                                           semantic_error_generic_impl_parameter_mismatch, argument.identifier.source));
            return evaluate_expression_result_empty;
        }
    }

    evaluate_expression_result const generic =
        evaluate_expression(state, function, *interface_expression_.generic, NULL,
                            type_expectation_create_exact(optional_type_create_empty()));
    switch (generic.status)
    {
    case evaluation_status_value:
        break;

    case evaluation_status_error:
        return evaluate_expression_result_empty;

    case evaluation_status_exit:
    case evaluation_status_return:
        LPG_TO_DO();
    }
    if (!generic.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    if (generic.compile_time_value.value_.kind != value_kind_generic_interface)
    {
        LPG_TO_DO();
    }

    generic_closures const closures = find_generic_closures_in_impl(state, tree);

    program_check *const root = state->root;
    generic_interface *const interface_ =
        &root->generic_interfaces[generic.compile_time_value.value_.generic_interface];
    interface_->generic_impls = reallocate_array(
        interface_->generic_impls, (interface_->generic_impl_count + 1), sizeof(*interface_->generic_impls));
    interface_->generic_impls[interface_->generic_impl_count] =
        generic_impl_create(impl_expression_clone(tree), closures,
                            generic_impl_self_create_generic(generic_instantiation_expression_clone(self)),
                            state->source, unicode_view_copy(state->current_import_directory));
    interface_->generic_impl_count += 1;
    return make_unit(&state->used_registers, function);
}

static evaluate_expression_result evaluate_generic_impl(function_checking_state *state,
                                                        instruction_sequence *const function,
                                                        impl_expression const element)
{
    if (element.self->type == expression_type_generic_instantiation)
    {
        if (element.interface_->type == expression_type_generic_instantiation)
        {
            return evaluate_fully_generic_impl(state, function, element.interface_->generic_instantiation,
                                               element.self->generic_instantiation, element);
        }
        else
        {
            return evaluate_generic_impl_regular_interface(
                state, function, *element.interface_, element.self->generic_instantiation, element);
        }
    }
    else
    {
        if (element.interface_->type == expression_type_generic_instantiation)
        {
            return evaluate_generic_impl_regular_self(state, function, element);
        }
        else
        {
            LPG_TO_DO();
        }
    }
}

static optional_size find_method(lpg_interface const *const in, unicode_view const name)
{
    for (size_t i = 0; i < in->method_count; ++i)
    {
        if (unicode_view_equals(unicode_view_from_string(in->methods[i].name), name))
        {
            return make_optional_size(i);
        }
    }
    return optional_size_empty;
}

static optional_size evaluate_impl_core(function_checking_state *state, instruction_sequence *const function,
                                        impl_expression_method const *const method_trees,
                                        size_t const defined_method_count, type const self,
                                        interface_id const target_interface, source_location const self_source,
                                        source_location const impl_begin)
{
    size_t impl_id;
    {
        lpg_interface *const implemented_interface = &state->program->interfaces[target_interface];
        for (size_t i = 0; i < implemented_interface->implementation_count; ++i)
        {
            if (type_equals(self, implemented_interface->implementations[i].self))
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_duplicate_impl, self_source));
                return optional_size_empty;
            }
        }
        impl_id = begin_implementation(implemented_interface, self);
    }

    function_pointer_value *const methods =
        allocate_array(state->program->interfaces[target_interface].method_count, sizeof(*methods));
    for (size_t i = 0; i < defined_method_count; ++i)
    {
        lpg_interface *const implemented_interface = &state->program->interfaces[target_interface];
        optional_size const canonical_method_index =
            find_method(implemented_interface, unicode_view_from_string(method_trees[i].name.value));
        if (canonical_method_index.state == optional_empty)
        {
            emit_semantic_error(state, semantic_error_create(semantic_error_extra_method, method_trees[i].name.source));
            if (methods)
            {
                deallocate(methods);
            }
            return optional_size_empty;
        }
        method_evaluation_result const method =
            evaluate_method_definition(state, function, method_trees[i], self,
                                       implemented_interface->methods[canonical_method_index.value_if_set].result);
        if (!method.is_success)
        {
            if (methods)
            {
                deallocate(methods);
            }
            return optional_size_empty;
        }
        methods[canonical_method_index.value_if_set] = method.success;
    }

    lpg_interface *const implemented_interface = &state->program->interfaces[target_interface];
    if (defined_method_count < implemented_interface->method_count)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_method, impl_begin));
        if (methods)
        {
            deallocate(methods);
        }
        return optional_size_empty;
    }

    finish_implementation(
        implemented_interface, impl_id, implementation_create(methods, implemented_interface->method_count));
    return make_optional_size(impl_id);
}

static evaluate_expression_result evaluate_impl(function_checking_state *state, instruction_sequence *const function,
                                                impl_expression const element)
{
    if (element.generic_parameters.count > 0)
    {
        return evaluate_generic_impl(state, function, element);
    }

    instruction_checkpoint const previous_code = make_checkpoint(state, function);

    compile_time_type_expression_result const interface_evaluated =
        expect_compile_time_type(state, function, *element.interface_);
    if (!interface_evaluated.has_value)
    {
        return evaluate_expression_result_empty;
    }
    if (interface_evaluated.compile_time_value.kind != type_kind_interface)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_expected_interface,
                                                         expression_source_begin(*element.interface_)));
        return evaluate_expression_result_empty;
    }

    compile_time_type_expression_result const self_evaluated = expect_compile_time_type(state, function, *element.self);
    if (!self_evaluated.has_value)
    {
        return evaluate_expression_result_empty;
    }

    type const self = self_evaluated.compile_time_value;

    restore(previous_code);
    evaluate_impl_core(state, function, element.methods, element.method_count, self,
                       interface_evaluated.compile_time_value.interface_, expression_source_begin(*element.self),
                       element.begin);
    return make_unit(&state->used_registers, function);
}

static evaluate_expression_result evaluate_instantiate_struct(function_checking_state *const state,
                                                              instruction_sequence *const function,
                                                              instantiate_struct_expression const element)
{
    evaluate_expression_result const type_evaluated =
        evaluate_expression(state, function, *element.type, NULL,
                            type_expectation_create_exact(optional_type_create_set(type_from_type())));
    if (type_evaluated.status != evaluation_status_value)
    {
        return type_evaluated;
    }
    if (!type_evaluated.compile_time_value.is_set)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                         expression_source_begin(*element.type)));
        return evaluate_expression_result_empty;
    }
    if (type_evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                         expression_source_begin(*element.type)));
        return evaluate_expression_result_empty;
    }
    if (type_evaluated.compile_time_value.value_.type_.kind != type_kind_structure)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_expected_structure, expression_source_begin(*element.type)));
        return evaluate_expression_result_empty;
    }
    struct_id const structure_id = type_evaluated.compile_time_value.value_.type_.structure_;
    ASSUME(structure_id < state->program->struct_count);
    structure const *const instantiated_structure = state->program->structs + structure_id;
    if (element.arguments.length < instantiated_structure->count)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_missing_argument, expression_source_begin(*element.type)));
        return evaluate_expression_result_empty;
    }
    if (element.arguments.length > instantiated_structure->count)
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_extraneous_argument, expression_source_begin(*element.type)));
        return evaluate_expression_result_empty;
    }
    evaluate_struct_arguments_result const arguments_evaluated = evaluate_struct_arguments(
        state, function, element.arguments.elements, element.arguments.length, instantiated_structure->members);
    if (!arguments_evaluated.registers)
    {
        return evaluate_expression_result_empty;
    }
    value *const compile_time_values = garbage_collector_allocate_array(
        &state->program->memory, element.arguments.length, sizeof(*compile_time_values));
    bool is_pure = true;
    for (register_id i = 0; i < element.arguments.length; ++i)
    {
        optional_value const current_element =
            read_register_compile_time_value(state, arguments_evaluated.registers[i]);
        if (!current_element.is_set)
        {
            is_pure = false;
            break;
        }
        compile_time_values[i] = current_element.value_;
    }
    register_id const into = allocate_register(&state->used_registers);
    if (is_pure)
    {
        deallocate(arguments_evaluated.registers);
        value const compile_time_value =
            value_from_structure(structure_value_create(compile_time_values, element.arguments.length));
        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      into, compile_time_value, type_from_struct(structure_id))));
        write_register_compile_time_value(state, into, compile_time_value);
        return evaluate_expression_result_create(evaluation_status_value, into, type_from_struct(structure_id),
                                                 optional_value_create(compile_time_value), true);
    }
    add_instruction(function, instruction_create_instantiate_struct(instantiate_struct_instruction_create(
                                  into, structure_id, arguments_evaluated.registers, element.arguments.length)));
    return evaluate_expression_result_create(
        evaluation_status_value, into, type_from_struct(structure_id), optional_value_empty, false);
}

static evaluate_expression_result evaluate_type_of(function_checking_state *const state,
                                                   instruction_sequence *const function,
                                                   type_of_expression const element)
{
    instruction_checkpoint const before = make_checkpoint(state, function);
    evaluate_expression_result const target_evaluated = evaluate_expression(
        state, function, *element.target, NULL, type_expectation_create_exact(optional_type_create_empty()));
    restore(before);
    register_id const where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  where, value_from_type(target_evaluated.type_), type_from_type())));
    write_register_compile_time_value(state, where, value_from_type(target_evaluated.type_));
    return evaluate_expression_result_create(evaluation_status_value, where, type_from_type(),
                                             optional_value_create(value_from_type(target_evaluated.type_)), true);
}

static generic_closures find_generic_closures(function_checking_state *state, enum_expression const definition)
{
    generic_closures result = {NULL, 0};
    for (size_t i = 0; i < definition.element_count; ++i)
    {
        if (definition.elements[i].state)
        {
            find_generic_closures_in_expression(&result, state, *definition.elements[i].state);
        }
    }
    return result;
}

static evaluate_expression_result evaluate_generic_enum_expression(function_checking_state *state,
                                                                   instruction_sequence *function,
                                                                   enum_expression const element)
{
    program_check *const root = state->root;
    root->generic_enums =
        reallocate_array(root->generic_enums, root->generic_enum_count + 1, sizeof(*root->generic_enums));
    generic_enum_id const id = root->generic_enum_count;
    generic_closures const closures = find_generic_closures(state, element);
    root->generic_enums[id] = generic_enum_create(
        enum_expression_clone(element), closures, unicode_view_copy(state->current_import_directory));
    root->generic_enum_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  into, value_from_generic_enum(id), type_from_generic_enum())));
    write_register_compile_time_value(state, into, value_from_generic_enum(id));
    return evaluate_expression_result_create(evaluation_status_value, into, type_from_generic_enum(),
                                             optional_value_create(value_from_generic_enum(id)), true);
}

static evaluate_expression_result
evaluate_enum_expression(function_checking_state *state, instruction_sequence *function, enum_expression const element)
{
    if (element.parameters.count > 0)
    {
        return evaluate_generic_enum_expression(state, function, element);
    }
    enum_id const new_enum_id = state->program->enum_count;
    state->program->enum_count += 1;
    state->program->enums = reallocate_array(state->program->enums, (new_enum_id + 1), sizeof(*state->program->enums));
    state->program->enums[new_enum_id] = enumeration_create(NULL, 0);
    enumeration_element *const elements = allocate_array(element.element_count, sizeof(*elements));
    for (size_t i = 0; i < element.element_count; ++i)
    {
        optional_type element_state = optional_type_create_empty();
        if (element.elements[i].state)
        {
            expression const state_expr = *element.elements[i].state;
            evaluate_expression_result const state_evaluated =
                evaluate_expression(state, function, state_expr, NULL,
                                    type_expectation_create_exact(optional_type_create_set(type_from_type())));
            if (state_evaluated.status == evaluation_status_value)
            {
                if (!state_evaluated.compile_time_value.is_set ||
                    (state_evaluated.compile_time_value.value_.kind != value_kind_type))
                {
                    emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                                     expression_source_begin(state_expr)));
                    for (size_t m = 0; m < i; ++m)
                    {
                        enumeration_element_free(elements + m);
                    }
                    deallocate(elements);
                    return evaluate_expression_result_empty;
                }
                element_state = optional_type_create_set(state_evaluated.compile_time_value.value_.type_);
            }
            else
            {
                for (size_t m = 0; m < i; ++m)
                {
                    enumeration_element_free(elements + m);
                }
                deallocate(elements);
                return evaluate_expression_result_empty;
            }
        }

        elements[i] = enumeration_element_create(
            unicode_view_copy(unicode_view_from_string(element.elements[i].name)), element_state);
        for (size_t k = 0; k < i; ++k)
        {
            if (unicode_string_equals(element.elements[i].name, element.elements[k].name))
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_duplicate_enum_element, element.begin));
                for (size_t m = 0; m <= i; ++m)
                {
                    enumeration_element_free(elements + m);
                }
                deallocate(elements);
                return evaluate_expression_result_empty;
            }
        }
    }
    enumeration *const destination = &state->program->enums[new_enum_id];
    ASSUME(destination->size == 0);
    ASSUME(destination->elements == NULL);
    *destination = enumeration_create(elements, element.element_count);
    register_id const into = allocate_register(&state->used_registers);
    value const literal = value_from_type(type_from_enumeration(new_enum_id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, literal, type_from_type())));
    write_register_compile_time_value(state, into, literal);
    return evaluate_expression_result_create(
        evaluation_status_value, into, type_from_type(), optional_value_create(literal), true);
}

static bool values_equal(value const *const first, value const *const second, size_t const count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!value_equals(first[i], second[i]))
        {
            return false;
        }
    }
    return true;
}

static evaluate_expression_result instantiate_generic_enum(function_checking_state *const state,
                                                           instruction_sequence *const function,
                                                           generic_enum_id const generic, value *const arguments,
                                                           size_t const argument_count, type *const argument_types,
                                                           source_location const where)
{
    program_check *const root = state->root;
    generic_enum const instantiated_enum = root->generic_enums[generic];
    if (argument_count < instantiated_enum.tree.parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_argument, where));
        return evaluate_expression_result_empty;
    }
    if (argument_count > instantiated_enum.tree.parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_extraneous_argument, where));
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < root->enum_instantiation_count; ++i)
    {
        generic_enum_instantiation *const instantiation = root->enum_instantiations + i;
        if (instantiation->generic != generic)
        {
            continue;
        }
        if (argument_count != instantiation->argument_count)
        {
            LPG_UNREACHABLE();
        }
        if (!values_equal(instantiation->arguments, arguments, argument_count))
        {
            continue;
        }
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        register_id const into = allocate_register(&state->used_registers);
        value const literal = value_from_type(type_from_enumeration(instantiation->instantiated));
        add_instruction(
            function, instruction_create_literal(literal_instruction_create(into, literal, type_from_type())));
        write_register_compile_time_value(state, into, literal);
        return evaluate_expression_result_create(
            evaluation_status_value, into, type_from_type(), optional_value_create(literal), true);
    }
    enum_expression const original = instantiated_enum.tree;
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state enum_checking = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored_instructions,
        optional_type_create_empty(), false, state->source,
        unicode_view_from_string(instantiated_enum.current_import_directory));
    for (size_t i = 0; i < instantiated_enum.tree.parameters.count; ++i)
    {
        register_id const argument_register = allocate_register(&enum_checking.used_registers);
        add_local_variable(&enum_checking.local_variables,
                           local_variable_create(
                               unicode_view_copy(unicode_view_from_string(instantiated_enum.tree.parameters.names[i])),
                               local_variable_phase_initialized, argument_types[i], optional_value_create(arguments[i]),
                               argument_register, NULL));
        write_register_compile_time_value(&enum_checking, argument_register, arguments[i]);
    }
    use_generic_closures(&enum_checking, instantiated_enum.closures);
    evaluate_expression_result const evaluated =
        evaluate_enum_expression(&enum_checking, &ignored_instructions,
                                 enum_expression_create(original.begin, generic_parameter_list_create(NULL, 0),
                                                        original.elements, original.element_count));
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(enum_checking.local_variables);
    if (enum_checking.register_compile_time_values)
    {
        deallocate(enum_checking.register_compile_time_values);
    }
    switch (evaluated.status)
    {
    case evaluation_status_value:
        break;

    case evaluation_status_error:
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        return evaluate_expression_result_empty;

    case evaluation_status_exit:
    case evaluation_status_return:
        LPG_TO_DO();
    }
    if (!evaluated.compile_time_value.is_set)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.type_.kind != type_kind_enumeration)
    {
        LPG_UNREACHABLE();
    }
    size_t const id = root->enum_instantiation_count;
    root->enum_instantiations = reallocate_array(
        root->enum_instantiations, root->enum_instantiation_count + 1, sizeof(*root->enum_instantiations));
    root->enum_instantiations[id] = generic_enum_instantiation_create(
        generic, argument_types, arguments, argument_count, evaluated.compile_time_value.value_.type_.enum_);
    root->enum_instantiation_count += 1;
    register_id const result_where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  result_where, evaluated.compile_time_value.value_, evaluated.type_)));
    return evaluate_expression_result_create(evaluation_status_value, result_where, evaluated.type_,
                                             optional_value_create(evaluated.compile_time_value.value_), true);
}

static evaluate_expression_result instantiate_generic_struct(function_checking_state *const state,
                                                             instruction_sequence *const function,
                                                             generic_struct_id const generic, value *const arguments,
                                                             size_t const argument_count, type *const argument_types,
                                                             source_location const where)
{
    program_check *const root = state->root;
    generic_struct const instantiated_struct = root->generic_structs[generic];
    if (argument_count < instantiated_struct.tree.generic_parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_argument, where));
        return evaluate_expression_result_empty;
    }
    if (argument_count > instantiated_struct.tree.generic_parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_extraneous_argument, where));
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < root->struct_instantiation_count; ++i)
    {
        generic_struct_instantiation *const instantiation = root->struct_instantiations + i;
        if (instantiation->generic != generic)
        {
            continue;
        }
        if (argument_count != instantiation->argument_count)
        {
            LPG_UNREACHABLE();
        }
        if (!values_equal(instantiation->arguments, arguments, argument_count))
        {
            continue;
        }
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        register_id const into = allocate_register(&state->used_registers);
        value const literal = value_from_type(type_from_struct(instantiation->instantiated));
        add_instruction(
            function, instruction_create_literal(literal_instruction_create(into, literal, type_from_type())));
        write_register_compile_time_value(state, into, literal);
        return evaluate_expression_result_create(
            evaluation_status_value, into, type_from_type(), optional_value_create(literal), true);
    }
    struct_expression const original = instantiated_struct.tree;
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state struct_checking = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored_instructions,
        optional_type_create_empty(), false, state->source,
        unicode_view_from_string(instantiated_struct.current_import_directory));
    for (size_t i = 0; i < instantiated_struct.tree.generic_parameters.count; ++i)
    {
        register_id const argument_register = allocate_register(&struct_checking.used_registers);
        add_local_variable(
            &struct_checking.local_variables,
            local_variable_create(
                unicode_view_copy(unicode_view_from_string(instantiated_struct.tree.generic_parameters.names[i])),
                local_variable_phase_initialized, argument_types[i], optional_value_create(arguments[i]),
                argument_register, NULL));
        write_register_compile_time_value(&struct_checking, argument_register, arguments[i]);
    }
    use_generic_closures(&struct_checking, instantiated_struct.closures);
    evaluate_expression_result const evaluated =
        evaluate_struct(&struct_checking, &ignored_instructions,
                        struct_expression_create(generic_parameter_list_create(NULL, 0), original.source,
                                                 original.elements, original.element_count),
                        NULL);
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(struct_checking.local_variables);
    if (struct_checking.register_compile_time_values)
    {
        deallocate(struct_checking.register_compile_time_values);
    }
    if (evaluated.status != evaluation_status_value)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        return evaluate_expression_result_empty;
    }
    if (!evaluated.compile_time_value.is_set)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.type_.kind != type_kind_structure)
    {
        LPG_UNREACHABLE();
    }
    size_t const id = root->struct_instantiation_count;
    root->struct_instantiations = reallocate_array(
        root->struct_instantiations, root->struct_instantiation_count + 1, sizeof(*root->struct_instantiations));
    root->struct_instantiations[id] = generic_struct_instantiation_create(
        generic, arguments, argument_count, evaluated.compile_time_value.value_.type_.structure_, argument_types);
    root->struct_instantiation_count += 1;
    register_id const result_where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  result_where, evaluated.compile_time_value.value_, evaluated.type_)));
    return evaluate_expression_result_create(evaluation_status_value, result_where, evaluated.type_,
                                             optional_value_create(evaluated.compile_time_value.value_), true);
}

static evaluate_expression_result instantiate_generic_interface(function_checking_state *const state,
                                                                instruction_sequence *const function,
                                                                generic_interface_id const generic,
                                                                value *const arguments, size_t const argument_count,
                                                                type *const argument_types, source_location const where)
{
    program_check *const root = state->root;
    generic_interface const instantiated_interface = root->generic_interfaces[generic];
    if (argument_count < instantiated_interface.tree.parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_argument, where));
        return evaluate_expression_result_empty;
    }
    if (argument_count > instantiated_interface.tree.parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_extraneous_argument, where));
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < root->interface_instantiation_count; ++i)
    {
        generic_interface_instantiation *const instantiation = root->interface_instantiations + i;
        if (instantiation->generic != generic)
        {
            continue;
        }
        if (argument_count != instantiation->argument_count)
        {
            LPG_UNREACHABLE();
        }
        if (!values_equal(instantiation->arguments, arguments, argument_count))
        {
            continue;
        }
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        register_id const into = allocate_register(&state->used_registers);
        value const literal = value_from_type(type_from_interface(instantiation->instantiated));
        add_instruction(
            function, instruction_create_literal(literal_instruction_create(into, literal, type_from_type())));
        write_register_compile_time_value(state, into, literal);
        return evaluate_expression_result_create(
            evaluation_status_value, into, type_from_type(), optional_value_create(literal), true);
    }
    interface_expression const original = instantiated_interface.tree;
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state interface_checking = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored_instructions,
        optional_type_create_empty(), false, state->source,
        unicode_view_from_string(instantiated_interface.current_import_directory));
    size_t const instantiation_id = root->interface_instantiation_count;
    root->interface_instantiations =
        reallocate_array(root->interface_instantiations, root->interface_instantiation_count + 1,
                         sizeof(*root->interface_instantiations));
    root->interface_instantiations[instantiation_id] = generic_interface_instantiation_create(
        generic, argument_types, arguments, argument_count, state->program->interface_count);
    root->interface_instantiation_count += 1;

    for (size_t i = 0; i < instantiated_interface.tree.parameters.count; ++i)
    {
        register_id const argument_register = allocate_register(&interface_checking.used_registers);
        add_local_variable(
            &interface_checking.local_variables,
            local_variable_create(
                unicode_view_copy(unicode_view_from_string(instantiated_interface.tree.parameters.names[i])),
                local_variable_phase_initialized, argument_types[i], optional_value_create(arguments[i]),
                argument_register, NULL));
        write_register_compile_time_value(&interface_checking, argument_register, arguments[i]);
    }
    use_generic_closures(&interface_checking, instantiated_interface.closures);
    evaluate_expression_result const evaluated =
        evaluate_interface(&interface_checking, &ignored_instructions,
                           interface_expression_create(generic_parameter_list_create(NULL, 0), original.source,
                                                       original.methods, original.method_count),
                           NULL);
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(interface_checking.local_variables);
    if (interface_checking.register_compile_time_values)
    {
        deallocate(interface_checking.register_compile_time_values);
    }
    if (evaluated.status != evaluation_status_value)
    {
        return evaluate_expression_result_empty;
    }
    if (!evaluated.compile_time_value.is_set)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.type_.kind != type_kind_interface)
    {
        LPG_UNREACHABLE();
    }
    ASSUME(instantiation_id < root->interface_instantiation_count);
    ASSUME(evaluated.compile_time_value.value_.type_.interface_ ==
           root->interface_instantiations[instantiation_id].instantiated);

    register_id const result_where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  result_where, evaluated.compile_time_value.value_, evaluated.type_)));
    return evaluate_expression_result_create(evaluation_status_value, result_where, evaluated.type_,
                                             optional_value_create(evaluated.compile_time_value.value_), true);
}

static evaluate_expression_result instantiate_generic_lambda(function_checking_state *const state,
                                                             instruction_sequence *const function,
                                                             generic_lambda_id const generic, value *const arguments,
                                                             size_t const argument_count, type *const argument_types,
                                                             source_location const where)
{
    program_check *const root = state->root;
    generic_lambda const instantiated_lambda = root->generic_lambdas[generic];
    if (argument_count < instantiated_lambda.tree.generic_parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_missing_argument, where));
        return evaluate_expression_result_empty;
    }
    if (argument_count > instantiated_lambda.tree.generic_parameters.count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_extraneous_argument, where));
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < root->lambda_instantiation_count; ++i)
    {
        generic_lambda_instantiation *const instantiation = root->lambda_instantiations + i;
        if (instantiation->generic != generic)
        {
            continue;
        }
        if (argument_count != instantiation->argument_count)
        {
            LPG_UNREACHABLE();
        }
        if (!values_equal(instantiation->arguments, arguments, argument_count))
        {
            continue;
        }
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        register_id const into = allocate_register(&state->used_registers);
        value const literal =
            value_from_function_pointer(function_pointer_value_from_internal(instantiation->instantiated, NULL, 0));
        type const function_type = type_from_lambda(lambda_type_create(instantiation->instantiated));
        add_instruction(function, instruction_create_literal(literal_instruction_create(into, literal, function_type)));
        write_register_compile_time_value(state, into, literal);
        return evaluate_expression_result_create(
            evaluation_status_value, into, function_type, optional_value_create(literal), true);
    }
    lambda const original = instantiated_lambda.tree;
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state lambda_checking = function_checking_state_create(
        state->root, NULL, false, state->global, state->on_error, state->user, state->program, &ignored_instructions,
        optional_type_create_empty(), false, instantiated_lambda.file,
        unicode_view_from_string(instantiated_lambda.current_import_directory));
    for (size_t i = 0; i < instantiated_lambda.tree.generic_parameters.count; ++i)
    {
        register_id const argument_register = allocate_register(&lambda_checking.used_registers);
        add_local_variable(
            &lambda_checking.local_variables,
            local_variable_create(
                unicode_view_copy(unicode_view_from_string(instantiated_lambda.tree.generic_parameters.names[i])),
                local_variable_phase_initialized, argument_types[i], optional_value_create(arguments[i]),
                argument_register, NULL));
        write_register_compile_time_value(&lambda_checking, argument_register, arguments[i]);
    }
    use_generic_closures(&lambda_checking, instantiated_lambda.closures);

    function_id const this_lambda_id = reserve_function_id(state);
    {
        size_t const instantiation_id = root->lambda_instantiation_count;
        root->lambda_instantiations = reallocate_array(
            root->lambda_instantiations, root->lambda_instantiation_count + 1, sizeof(*root->lambda_instantiations));
        root->lambda_instantiations[instantiation_id] =
            generic_lambda_instantiation_create(generic, arguments, argument_count, this_lambda_id);
        root->lambda_instantiation_count += 1;
    }

    evaluate_expression_result const evaluated = evaluate_lambda(
        &lambda_checking, &ignored_instructions,
        lambda_create(generic_parameter_list_create(NULL, 0), original.header, original.result, original.source), NULL,
        optional_function_id_create(this_lambda_id));
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(lambda_checking.local_variables);
    if (lambda_checking.register_compile_time_values)
    {
        deallocate(lambda_checking.register_compile_time_values);
    }
    if (evaluated.status != evaluation_status_value)
    {
        if (argument_types)
        {
            deallocate(argument_types);
        }
        return evaluate_expression_result_empty;
    }
    if (!evaluated.compile_time_value.is_set)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.compile_time_value.value_.kind != value_kind_function_pointer)
    {
        LPG_UNREACHABLE();
    }
    if (evaluated.type_.kind != type_kind_lambda)
    {
        LPG_UNREACHABLE();
    }
    ASSUME(evaluated.compile_time_value.value_.function_pointer.code == this_lambda_id);
    ASSUME(!evaluated.compile_time_value.value_.function_pointer.external);

    if (argument_types)
    {
        deallocate(argument_types);
    }
    register_id const result_where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  result_where, evaluated.compile_time_value.value_, evaluated.type_)));
    return evaluate_expression_result_create(evaluation_status_value, result_where, evaluated.type_,
                                             optional_value_create(evaluated.compile_time_value.value_), true);
}

static evaluate_expression_result evaluate_generic_instantiation(function_checking_state *const state,
                                                                 instruction_sequence *const function,
                                                                 generic_instantiation_expression const element)
{
    evaluate_expression_result const generic_evaluated = evaluate_expression(
        state, function, *element.generic, NULL, type_expectation_create_exact(optional_type_create_empty()));
    if (generic_evaluated.status != evaluation_status_value)
    {
        return evaluate_expression_result_empty;
    }
    value *const arguments = allocate_array(element.count, sizeof(*arguments));
    type *const argument_types = allocate_array(element.count, sizeof(*argument_types));
    for (size_t i = 0; i < element.count; ++i)
    {
        evaluate_expression_result const argument_evaluated = evaluate_expression(
            state, function, element.arguments[i], NULL, type_expectation_create_exact(optional_type_create_empty()));
        if (argument_evaluated.status != evaluation_status_value)
        {
            if (arguments)
            {
                deallocate(arguments);
            }
            if (argument_types)
            {
                deallocate(argument_types);
            }
            return evaluate_expression_result_empty;
        }
        if (!argument_evaluated.compile_time_value.is_set)
        {
            if (arguments)
            {
                deallocate(arguments);
            }
            if (argument_types)
            {
                deallocate(argument_types);
            }
            emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_value,
                                                             expression_source_begin(element.arguments[i])));
            return evaluate_expression_result_empty;
        }
        arguments[i] = argument_evaluated.compile_time_value.value_;
        argument_types[i] = argument_evaluated.type_;
    }
    if (!generic_evaluated.compile_time_value.is_set)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_value,
                                                         expression_source_begin(*element.generic)));
        return evaluate_expression_result_empty;
    }
    if (generic_evaluated.compile_time_value.value_.kind == value_kind_generic_enum)
    {
        return instantiate_generic_enum(state, function, generic_evaluated.compile_time_value.value_.generic_enum,
                                        arguments, element.count, argument_types,
                                        expression_source_begin(*element.generic));
    }
    if (generic_evaluated.compile_time_value.value_.kind == value_kind_generic_interface)
    {
        return instantiate_generic_interface(state, function,
                                             generic_evaluated.compile_time_value.value_.generic_interface, arguments,
                                             element.count, argument_types, expression_source_begin(*element.generic));
    }
    if (generic_evaluated.compile_time_value.value_.kind == value_kind_generic_lambda)
    {
        return instantiate_generic_lambda(state, function, generic_evaluated.compile_time_value.value_.generic_lambda,
                                          arguments, element.count, argument_types,
                                          expression_source_begin(*element.generic));
    }
    if (generic_evaluated.compile_time_value.value_.kind == value_kind_generic_struct)
    {
        return instantiate_generic_struct(state, function, generic_evaluated.compile_time_value.value_.generic_struct,
                                          arguments, element.count, argument_types,
                                          expression_source_begin(*element.generic));
    }
    if (arguments)
    {
        deallocate(arguments);
    }
    if (argument_types)
    {
        deallocate(argument_types);
    }
    emit_semantic_error(
        state, semantic_error_create(semantic_error_expected_generic_type, expression_source_begin(*element.generic)));
    return evaluate_expression_result_empty;
}

static evaluate_expression_result evaluate_import(function_checking_state *const state,
                                                  instruction_sequence *const function, import_expression const element)
{
    for (size_t i = 0; i < state->root->module_count; ++i)
    {
        module const current_module = state->root->modules[i];
        if (unicode_view_equals(
                unicode_view_from_string(current_module.name), unicode_view_from_string(element.name.value)))
        {
            if (!current_module.content.is_set)
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_import_failed, element.begin));
                return evaluate_expression_result_empty;
            }
            ASSUME(current_module.schema.is_set);
            register_id const where = allocate_register(&state->used_registers);
            write_register_compile_time_value(state, where, current_module.content.value_);
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          where, current_module.content.value_, current_module.schema.value)));
            return evaluate_expression_result_create(evaluation_status_value, where, current_module.schema.value,
                                                     optional_value_create(current_module.content.value_), true);
        }
    }
    begin_load_module(state->root, unicode_view_copy(unicode_view_from_string(element.name.value)));
    load_module_result const imported = load_module(state, unicode_view_from_string(element.name.value));
    if (!imported.loaded.is_set)
    {
        fail_load_module(state->root, unicode_view_from_string(element.name.value));
        emit_semantic_error(state, semantic_error_create(semantic_error_import_failed, element.begin));
        return evaluate_expression_result_empty;
    }
    succeed_load_module(
        state->root, unicode_view_from_string(element.name.value), imported.loaded.value_, imported.schema);
    register_id const where = allocate_register(&state->used_registers);
    write_register_compile_time_value(state, where, imported.loaded.value_);
    add_instruction(function, instruction_create_literal(
                                  literal_instruction_create(where, imported.loaded.value_, imported.schema)));
    return evaluate_expression_result_create(
        evaluation_status_value, where, imported.schema, optional_value_create(imported.loaded.value_), true);
}

static evaluate_expression_result evaluate_new_array(function_checking_state *const state,
                                                     instruction_sequence *const function,
                                                     new_array_expression const element)
{
    evaluate_expression_result const element_evaluated = evaluate_expression(
        state, function, *element.element, NULL, type_expectation_create_exact(optional_type_create_empty()));
    if (element_evaluated.status != evaluation_status_value)
    {
        return element_evaluated;
    }
    if (!element_evaluated.compile_time_value.is_set)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                         expression_source_begin(*element.element)));
        return evaluate_expression_result_empty;
    }
    unicode_string array_module_name = {"array", 0};
    array_module_name.length = strlen(array_module_name.data);
    evaluate_expression_result const array_imported = evaluate_import(
        state, function, import_expression_create(expression_source_begin(*element.element),
                                                  identifier_expression_create(
                                                      array_module_name, expression_source_begin(*element.element))));
    if (array_imported.status != evaluation_status_value)
    {
        LPG_TO_DO();
    }
    if (!array_imported.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    if (array_imported.compile_time_value.value_.kind != value_kind_structure)
    {
        LPG_TO_DO();
    }
    if (array_imported.compile_time_value.value_.structure.count != 1)
    {
        LPG_TO_DO();
    }
    value const generic_array = array_imported.compile_time_value.value_.structure.members[0];
    if (generic_array.kind != value_kind_generic_interface)
    {
        LPG_TO_DO();
    }
    generic_interface_id const array_generic = generic_array.generic_interface;
    value *const arguments = allocate_array(1, sizeof(*arguments));
    arguments[0] = element_evaluated.compile_time_value.value_;
    type *const argument_types = allocate_array(1, sizeof(*argument_types));
    argument_types[0] = element_evaluated.type_;
    evaluate_expression_result const array_instantiated = instantiate_generic_interface(
        state, function, array_generic, arguments, 1, argument_types, expression_source_begin(*element.element));
    if (array_instantiated.status != evaluation_status_value)
    {
        LPG_TO_DO();
    }
    if (!array_instantiated.compile_time_value.is_set)
    {
        LPG_TO_DO();
    }
    if (array_instantiated.compile_time_value.value_.kind != value_kind_type)
    {
        LPG_TO_DO();
    }
    if (array_instantiated.compile_time_value.value_.type_.kind != type_kind_interface)
    {
        LPG_TO_DO();
    }
    register_id const into = allocate_register(&state->used_registers);
    ASSUME(element_evaluated.compile_time_value.value_.kind == value_kind_type);
    add_instruction(function, instruction_create_new_array(new_array_instruction_create(
                                  array_instantiated.compile_time_value.value_.type_.interface_, into,
                                  element_evaluated.compile_time_value.value_.type_)));
    return evaluate_expression_result_create(
        evaluation_status_value, into, array_instantiated.compile_time_value.value_.type_, optional_value_empty, false);
}

static evaluate_expression_result evaluate_declare(function_checking_state *const state,
                                                   instruction_sequence *const function, declare const element)
{
    bool const declaration_possible =
        !local_variable_name_exists(state->local_variables, unicode_view_from_string(element.name.value));
    if (declaration_possible)
    {
        add_local_variable(&state->local_variables,
                           local_variable_create(unicode_view_copy(unicode_view_from_string(element.name.value)),
                                                 local_variable_phase_declared, type_from_unit(), optional_value_empty,
                                                 ~(register_id)0, NULL));
    }
    else
    {
        emit_semantic_error(
            state, semantic_error_create(semantic_error_declaration_with_existing_name, element.name.source));
    }

    optional_type declared_type = optional_type_create_empty();
    if (element.optional_type)
    {
        instruction_checkpoint const previous_code = make_checkpoint(state, function);
        evaluate_expression_result const declared_type_evaluated =
            evaluate_expression(state, function, *element.optional_type, NULL,
                                type_expectation_create_exact(optional_type_create_set(type_from_type())));
        restore(previous_code);
        switch (declared_type_evaluated.status)
        {
        case evaluation_status_value:
            if (!declared_type_evaluated.compile_time_value.is_set ||
                declared_type_evaluated.type_.kind != type_kind_type)
            {
                emit_semantic_error(state, semantic_error_create(semantic_error_expected_compile_time_type,
                                                                 expression_source_begin(*element.optional_type)));
            }
            else
            {
                declared_type = optional_type_create_set(declared_type_evaluated.compile_time_value.value_.type_);
            }
            break;

        case evaluation_status_error:
            break;

        case evaluation_status_exit:
        case evaluation_status_return:
            LPG_TO_DO();
        }
    }

    instruction_checkpoint const before_initialization = make_checkpoint(state, function);
    unicode_view const name = unicode_view_from_string(element.name.value);
    evaluate_expression_result const initializer =
        evaluate_expression(state, function, *element.initializer, declaration_possible ? &name : NULL,
                            type_expectation_create_exact(declared_type));
    if (initializer.status != evaluation_status_value)
    {
        return evaluate_expression_result_empty;
    }

    register_id final_where = initializer.where;
    if (initializer.compile_time_value.is_set)
    {
        restore(before_initialization);
        final_where = allocate_register(&state->used_registers);
        write_register_compile_time_value(state, final_where, initializer.compile_time_value.value_);
        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      final_where, initializer.compile_time_value.value_, initializer.type_)));
    }

    type final_type = initializer.type_;
    optional_value final_compile_time_value = initializer.compile_time_value;
    if (declared_type.is_set)
    {
        conversion_result const converted =
            convert(state, function, final_where, initializer.type_, initializer.compile_time_value,
                    expression_source_begin(*element.initializer), declared_type.value, false);
        if (converted.ok == success_no)
        {
            return evaluate_expression_result_empty;
        }
        final_where = converted.where;
        final_type = declared_type.value;
        final_compile_time_value = converted.compile_time_value;
    }

    if (declaration_possible)
    {
        define_register_debug_name(state, final_where, unicode_view_copy(unicode_view_from_string(element.name.value)));
        local_variable_initialize(&state->local_variables, unicode_view_from_string(element.name.value), final_type,
                                  final_compile_time_value, final_where);
    }

    register_id const unit_goes_into = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(
                                  literal_instruction_create(unit_goes_into, value_from_unit(), type_from_unit())));
    write_register_compile_time_value(state, unit_goes_into, value_from_unit());
    evaluate_expression_result const result = {
        evaluation_status_value, type_from_unit(), optional_value_create(value_from_unit()), unit_goes_into, false};
    return result;
}

static evaluate_expression_result evaluate_expression_core(function_checking_state *const state,
                                                           instruction_sequence *const function,
                                                           expression const element,
                                                           unicode_view const *const early_initialized_variable,
                                                           type_expectation const expected_result_type)
{
    switch (element.type)
    {
    case expression_type_new_array:
        return evaluate_new_array(state, function, element.new_array);

    case expression_type_import:
        return evaluate_import(state, function, element.import);

    case expression_type_generic_instantiation:
        return evaluate_generic_instantiation(state, function, element.generic_instantiation);

    case expression_type_type_of:
        return evaluate_type_of(state, function, element.type_of);

    case expression_type_instantiate_struct:
        return evaluate_instantiate_struct(state, function, element.instantiate_struct);

    case expression_type_interface:
        return evaluate_interface(state, function, element.interface_, early_initialized_variable);

    case expression_type_impl:
        return evaluate_impl(state, function, element.impl);

    case expression_type_lambda:
        return evaluate_lambda(
            state, function, element.lambda, early_initialized_variable, optional_function_id_empty());

    case expression_type_call:
        return evaluate_call_expression(state, function, element.call, expected_result_type);

    case expression_type_enum:
        return evaluate_enum_expression(state, function, element.enum_);

    case expression_type_integer_literal:
    {
        register_id const where = allocate_register(&state->used_registers);
        type const what =
            type_from_integer_range(integer_range_create(element.integer_literal.value, element.integer_literal.value));
        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      where, value_from_integer(element.integer_literal.value), what)));
        write_register_compile_time_value(state, where, value_from_integer(element.integer_literal.value));
        return evaluate_expression_result_create(
            evaluation_status_value, where, what,
            optional_value_create(value_from_integer(element.integer_literal.value)), true);
    }

    case expression_type_access_structure:
    {
        instruction_checkpoint const previous_code = make_checkpoint(state, function);
        register_id const result = allocate_register(&state->used_registers);
        read_structure_element_result const element_read =
            read_element(state, function, *element.access_structure.object, &element.access_structure.member, result);
        if (!element_read.success)
        {
            restore(previous_code);
            return evaluate_expression_result_empty;
        }
        return evaluate_expression_result_create(
            evaluation_status_value, result, element_read.type_, element_read.compile_time_value, element_read.is_pure);
    }

    case expression_type_comment:
        return make_unit(&state->used_registers, function);

    case expression_type_not:
        return evaluate_not_expression(state, function, &element);

    case expression_type_match:
        return evaluate_match_expression(state, function, &element, expected_result_type);

    case expression_type_string:
    {
        register_id const result = allocate_register(&state->used_registers);
        unicode_view literal;
        if (element.string.value.data[0] == '"')
        {
            memory_writer decoded = {NULL, 0, 0};
            stream_writer const decoded_writer = memory_writer_erase(&decoded);
            decode_string_literal(unicode_view_from_string(element.string.value), decoded_writer);
            char *const copy = garbage_collector_allocate(&state->program->memory, decoded.used);
            if (decoded.used > 0)
            {
                memcpy(copy, decoded.data, decoded.used);
            }
            literal = unicode_view_create(copy, decoded.used);
            memory_writer_free(&decoded);
        }
        else
        {
            const size_t length = element.string.value.length;
            char *const copy = garbage_collector_allocate(&state->program->memory, length - 2);
            ASSUME(length >= 2);
            memcpy(copy, element.string.value.data + 1, length - 2);
            literal = unicode_view_create(copy, length - 2);
        }

        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      result, value_from_string(literal), type_from_string())));
        write_register_compile_time_value(state, result, value_from_string(literal));
        type const string_type = {type_kind_string, {0}};
        return evaluate_expression_result_create(
            evaluation_status_value, result, string_type, optional_value_create(value_from_string(literal)), true);
    }

    case expression_type_identifier:
    {
        unicode_view const name = unicode_view_from_string(element.identifier.value);
        evaluate_expression_result const address =
            read_variable(state, function, name, element.identifier.source, expected_result_type);
        return address;
    }

    case expression_type_return:
        return evaluate_return_expression(state, function, element.return_);

    case expression_type_loop:
    {
        instruction_sequence body = {NULL, 0};
        bool const previous_is_in_loop = state->is_in_loop;
        state->is_in_loop = true;
        evaluate_expression_result const loop_body_result = check_sequence(
            state, &body, element.loop_body, type_expectation_create_exact(optional_type_create_empty()));
        ASSUME(state->is_in_loop);
        state->is_in_loop = previous_is_in_loop;
        register_id const unit_goes_into = allocate_register(&state->used_registers);
        add_instruction(function, instruction_create_loop(loop_instruction_create(unit_goes_into, body)));
        evaluate_expression_result const loop_result = {
            loop_body_result.status, type_from_unit(), optional_value_create(value_from_unit()), unit_goes_into, true};
        return loop_result;
    }

    case expression_type_break:
    {
        register_id const into = allocate_register(&state->used_registers);
        if (state->is_in_loop)
        {
            add_instruction(function, instruction_create_break(into));
        }
        else
        {
            emit_semantic_error(
                state, semantic_error_create(semantic_error_break_outside_of_loop, expression_source_begin(element)));
        }
        return evaluate_expression_result_create(
            evaluation_status_value, into, type_from_unit(), optional_value_empty, false);
    }

    case expression_type_sequence:
        return check_sequence(state, function, element.sequence, expected_result_type);

    case expression_type_declare:
        return evaluate_declare(state, function, element.declare);

    case expression_type_struct:
        return evaluate_struct(state, function, element.struct_, early_initialized_variable);

    case expression_type_placeholder:
        emit_semantic_error(state, semantic_error_create(semantic_error_placeholder_not_supported_here,
                                                         expression_source_begin(element)));
        return evaluate_expression_result_empty;

    case expression_type_binary:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result evaluate_expression(function_checking_state *const state,
                                                      instruction_sequence *const function, expression const element,
                                                      unicode_view const *const early_initialized_variable,
                                                      type_expectation const expected_result_type)
{
    size_t const expression_recursion_limit = 100;
    if (state->root->expression_recursion_depth >= expression_recursion_limit)
    {
        emit_semantic_error(state, semantic_error_create(semantic_error_expression_recursion_limit_reached,
                                                         expression_source_begin(element)));
        return evaluate_expression_result_empty;
    }
    state->root->expression_recursion_depth += 1;
    ASSUME(state->register_debug_name_count <= state->used_registers);
    evaluate_expression_result const result =
        evaluate_expression_core(state, function, element, early_initialized_variable, expected_result_type);
    ASSUME(state->register_debug_name_count <= state->used_registers);
    state->root->expression_recursion_depth -= 1;
    return result;
}

static evaluate_expression_result evaluate_return_expression(function_checking_state *state, instruction_sequence *body,
                                                             const expression *returned)
{
    evaluate_expression_result const result =
        evaluate_expression(state, body, *returned, NULL, type_expectation_create_exact(state->return_type));
    if (result.status != evaluation_status_value)
    {
        return result;
    }
    if (state->return_type.is_set)
    {
        conversion_result const converted = convert(state, body, result.where, result.type_, result.compile_time_value,
                                                    expression_source_begin(*returned), state->return_type.value, true);
        switch (converted.ok)
        {
        case success_no:
            return evaluate_expression_result_empty;

        case success_yes:
        {
            state->return_type = optional_type_create_set(converted.result_type);
            register_id const unit_goes_into = allocate_register(&state->used_registers);
            add_instruction(
                body, instruction_create_return(return_instruction_create(converted.where, unit_goes_into)));
            return evaluate_expression_result_create(
                evaluation_status_return, unit_goes_into, type_from_unit(), optional_value_empty, false);
        }
        }
        LPG_UNREACHABLE();
    }
    else
    {
        state->return_type = optional_type_create_set(result.type_);
    }
    register_id const unit_goes_into = allocate_register(&state->used_registers);
    add_instruction(body, instruction_create_return(return_instruction_create(result.where, unit_goes_into)));
    return evaluate_expression_result_create(
        evaluation_status_return, unit_goes_into, type_from_unit(), optional_value_empty, false);
}

static evaluate_expression_result check_sequence(function_checking_state *const state,
                                                 instruction_sequence *const output, sequence const input,
                                                 type_expectation const expected_return_type)
{
    ASSUME(output);
    size_t const previous_number_of_variables = state->local_variables.count;
    return check_sequence_finish(state, output, input, previous_number_of_variables, expected_return_type);
}

static structure clone_structure(structure const original)
{
    structure_member *const members = allocate_array(original.count, sizeof(*members));
    for (size_t i = 0; i < original.count; ++i)
    {
        members[i] = structure_member_create(original.members[i].what,
                                             unicode_view_copy(unicode_view_from_string(original.members[i].name)),
                                             original.members[i].compile_time_value);
    }
    return structure_create(members, original.count);
}

checked_program check(sequence const root, structure const global, check_error_handler *on_error, module_loader *loader,
                      source_file source, unicode_view const current_import_directory,
                      size_t const max_compile_time_heap, void *user)
{
    size_t const max_recursion = 100;
    structure *const structures = allocate_array(1, sizeof(*structures));
    structures[0] = clone_structure(global);
    checked_program program = {NULL,
                               0,
                               structures,
                               1,
                               garbage_collector_create(max_compile_time_heap),
                               allocate_array(1, sizeof(*program.functions)),
                               1,
                               allocate_array(5, sizeof(*program.enums)),
                               5};
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(unicode_string_from_c_str("false"), optional_type_create_empty());
        elements[1] = enumeration_element_create(unicode_string_from_c_str("true"), optional_type_create_empty());
        program.enums[standard_library_enum_boolean] = enumeration_create(elements, 2);
    }
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(
            unicode_string_from_c_str("ok"), optional_type_create_set(type_from_integer_range(integer_range_max())));
        elements[1] = enumeration_element_create(unicode_string_from_c_str("underflow"), optional_type_create_empty());
        program.enums[standard_library_enum_subtract_result] = enumeration_create(elements, 2);
    }
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(
            unicode_string_from_c_str("ok"), optional_type_create_set(type_from_integer_range(integer_range_max())));
        elements[1] = enumeration_element_create(unicode_string_from_c_str("overflow"), optional_type_create_empty());
        program.enums[standard_library_enum_add_result] = enumeration_create(elements, 2);
    }
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(
            unicode_string_from_c_str("ok"), optional_type_create_set(type_from_integer_range(integer_range_create(
                                                 integer_create(0, 0), integer_create(0, UINT32_MAX)))));
        elements[1] = enumeration_element_create(unicode_string_from_c_str("overflow"), optional_type_create_empty());
        program.enums[standard_library_enum_add_u32_result] = enumeration_create(elements, 2);
    }
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(
            unicode_string_from_c_str("ok"), optional_type_create_set(type_from_integer_range(integer_range_create(
                                                 integer_create(0, 0), integer_create(0, UINT64_MAX)))));
        elements[1] = enumeration_element_create(unicode_string_from_c_str("overflow"), optional_type_create_empty());
        program.enums[standard_library_enum_add_u64_result] = enumeration_create(elements, 2);
    }
    size_t const globals_count = global.count;
    value *const globals = allocate_array(globals_count, sizeof(*globals));
    for (size_t i = 0; i < globals_count; ++i)
    {
        optional_value const compile_time_global = global.members[i].compile_time_value;
        if (compile_time_global.is_set)
        {
            globals[i] = compile_time_global.value_;
        }
        else
        {
            /*TODO: solve properly*/
            globals[i] = value_from_unit();
        }
    }
    size_t current_recursion = 0;
    uint64_t const max_executed_instructions = 10000;
    uint64_t executed_instructions = 0;
    program_check check_root = {
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, loader, global, globals, NULL,
        NULL, 0, 0, interpreter_create(globals, &program.memory, &program.functions, &program.interfaces, max_recursion,
                                       &current_recursion, max_executed_instructions, &executed_instructions),
        NULL, 0};
    source_file_owning const source_copy = source_file_to_owning(source);
    check_function_result const checked =
        check_function(&check_root, NULL, expression_from_sequence(root), global, on_error, user, &program, NULL, NULL,
                       0, optional_type_create_empty(), true, optional_type_create_empty(), &source_copy,
                       current_import_directory, NULL, optional_function_id_create(0));
    deallocate(globals);
    source_file_owning_free(source_copy);
    if (checked.success)
    {
        ASSUME(checked.capture_count == 0);
        program.functions[0] = checked.function;
    }
    else
    {
        function_pointer *const dummy_signature = allocate(sizeof(*dummy_signature));
        *dummy_signature =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        program.functions[0] = checked_function_create(dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    program_check_free(check_root);
    return program;
}
