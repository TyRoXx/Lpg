#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_value.h"
#include "lpg_instruction.h"
#include "lpg_structure_member.h"
#include "lpg_string_literal.h"
#include "lpg_interpret.h"
#include <string.h>
#include "lpg_local_variable.h"
#include "lpg_instruction_checkpoint.h"
#include "lpg_function_checking_state.h"

check_function_result check_function_result_create(checked_function function, capture *captures, size_t capture_count)
{
    check_function_result const result = {true, function, captures, capture_count};
    return result;
}

check_function_result const check_function_result_empty = {false, {NULL, {NULL, 0}, NULL, 0}, NULL, 0};

typedef struct evaluate_expression_result
{
    bool has_value;
    register_id where;
    type type_;
    optional_value compile_time_value;
    bool is_pure;
    bool always_returns;
} evaluate_expression_result;

static evaluate_expression_result evaluate_expression_result_create(bool const has_value, register_id const where,
                                                                    type const type_,
                                                                    optional_value const compile_time_value,
                                                                    bool const is_pure, bool const always_returns)
{
    evaluate_expression_result const result = {has_value, where, type_, compile_time_value, is_pure, always_returns};
    return result;
}

static evaluate_expression_result const evaluate_expression_result_empty = {
    false, 0, {type_kind_type, {0}}, {false, {value_kind_integer, {NULL}}}, false, false};

static function_checking_state function_checking_state_create(
    program_check *const root, function_checking_state *parent, bool const may_capture_runtime_variables,
    structure const *global, check_error_handler *on_error, void *user, checked_program *const program,
    instruction_sequence *const body, optional_type const return_type, bool const has_declared_return_type)
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
                                            0};
    return result;
}

static evaluate_expression_result check_sequence(function_checking_state *const state,
                                                 instruction_sequence *const output, sequence const input);

static type get_parameter_type(type const callee, size_t const parameter, checked_function const *const all_functions,
                               enumeration const *const all_enums)
{
    switch (callee.kind)
    {
    case type_kind_lambda:
        ASSUME(parameter < all_functions[callee.lambda.lambda].signature->parameters.length);
        return all_functions[callee.lambda.lambda].signature->parameters.elements[parameter];

    case type_kind_function_pointer:
        ASSUME(parameter < callee.function_pointer_->parameters.length);
        return callee.function_pointer_->parameters.elements[parameter];

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
        LPG_UNREACHABLE();

    case type_kind_enum_constructor:
        ASSUME(parameter == 0);
        return all_enums[callee.enum_constructor->enumeration].elements[callee.enum_constructor->which].state;
    }
    LPG_UNREACHABLE();
}

typedef struct read_structure_element_result
{
    bool success;
    type type_;
    optional_value compile_time_value;
    bool is_pure;
} read_structure_element_result;

static read_structure_element_result read_structure_element_result_create(bool const success, type const type_,
                                                                          optional_value const compile_time_value,
                                                                          bool const is_pure)
{
    read_structure_element_result const result = {success, type_, compile_time_value, is_pure};
    return result;
}

static read_structure_element_result
read_structure_element(function_checking_state *const state, instruction_sequence *const function,
                       structure const *const type, register_id const where, unicode_view const element_name,
                       source_location const element_name_location, register_id const result)
{
    optional_value const compile_time_value = read_register_compile_time_value(state, where);
    LPG_FOR(struct_member_id, i, type->count)
    {
        if (!unicode_view_equals(element_name, unicode_view_from_string(type->members[i].name)))
        {
            continue;
        }
        if (compile_time_value.is_set)
        {
            ASSERT(compile_time_value.value_.kind == value_kind_flat_object);
            value const element_value = compile_time_value.value_.flat_object[i];
            add_instruction(function, instruction_create_literal(
                                          literal_instruction_create(result, element_value, type->members[i].what)));
            write_register_compile_time_value(state, result, element_value);
            return read_structure_element_result_create(
                true, type->members[i].what, optional_value_create(element_value), true);
        }
        add_instruction(function, instruction_create_read_struct(read_struct_instruction_create(where, i, result)));
        return read_structure_element_result_create(
            true, type->members[i].what, type->members[i].compile_time_value, true);
    }
    state->on_error(semantic_error_create(semantic_error_unknown_element, element_name_location), state->user);
    return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
}

static read_structure_element_result read_tuple_element(function_checking_state *state, instruction_sequence *function,
                                                        tuple_type const type, register_id const where,
                                                        unicode_view const element_name,
                                                        source_location const element_name_location,
                                                        register_id const result)
{
    integer element_index;
    if (!integer_parse(&element_index, element_name))
    {
        state->on_error(semantic_error_create(semantic_error_unknown_element, element_name_location), state->user);
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
    }
    if (!integer_less(element_index, integer_create(0, type.length)))
    {
        state->on_error(semantic_error_create(semantic_error_unknown_element, element_name_location), state->user);
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
    }
    ASSERT(integer_less(element_index, integer_create(0, ~(struct_member_id)0)));
    add_instruction(function, instruction_create_read_struct(
                                  read_struct_instruction_create(where, (struct_member_id)element_index.low, result)));
    return read_structure_element_result_create(true, type.elements[element_index.low], optional_value_empty, true);
}

static evaluate_expression_result evaluate_expression(function_checking_state *const state,
                                                      instruction_sequence *const function, expression const element);

static evaluate_expression_result evaluate_return_expression(function_checking_state *pState,
                                                             instruction_sequence *pSequence,
                                                             const expression *pExpression);

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
    method_description const method = state->program->interfaces[from_type].methods[method_index];
    *method_object = function_pointer_create(
        method.result, method.parameters, tuple_type_create(captures, 1), optional_type_create_empty());
    return read_structure_element_result_create(
        true, type_from_function_pointer(method_object), optional_value_empty, false);
}

static read_structure_element_result
read_interface_element(function_checking_state *state, instruction_sequence *function, register_id const from,
                       interface_id const from_type, unicode_view const element_name,
                       source_location const element_source, register_id const result)
{
    interface *const from_interface = state->program->interfaces + from_type;
    for (function_id i = 0; i < from_interface->method_count; ++i)
    {
        if (unicode_view_equals(unicode_view_from_string(from_interface->methods[i].name), element_name))
        {
            return read_interface_element_at(state, function, from, from_type, i, result);
        }
    }
    state->on_error(semantic_error_create(semantic_error_unknown_element, element_source), state->user);
    return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
}

static read_structure_element_result read_element(function_checking_state *state, instruction_sequence *function,
                                                  expression const object_tree,
                                                  const identifier_expression *const element, register_id const result)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, function);
    evaluate_expression_result const object = evaluate_expression(state, function, object_tree);
    if (!object.has_value)
    {
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
    }

    type const *const actual_type = &object.type_;
    switch (actual_type->kind)
    {
    case type_kind_structure:
        return read_structure_element(state, function, state->program->structs + actual_type->structure_, object.where,
                                      unicode_view_from_string(element->value), element->source, result);

    case type_kind_method_pointer:
    case type_kind_generic_enum:
        LPG_TO_DO();

    case type_kind_interface:
        return read_interface_element(state, function, object.where, actual_type->interface_,
                                      unicode_view_from_string(element->value), element->source, result);

    case type_kind_enum_constructor:
    case type_kind_integer_range:
    case type_kind_lambda:
    case type_kind_unit:
    case type_kind_function_pointer:
    case type_kind_string_ref:
        state->on_error(semantic_error_create(semantic_error_unknown_element, element->source), state->user);
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case type_kind_enumeration:
        state->on_error(
            semantic_error_create(semantic_error_no_members_on_enum_elements, element->source), state->user);
        return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

    case type_kind_tuple:
        return read_tuple_element(state, function, actual_type->tuple_, object.where,
                                  unicode_view_from_string(element->value), element->source, result);

    case type_kind_type:
    {
        if (!object.compile_time_value.is_set)
        {
            restore(previous_code);
            state->on_error(
                semantic_error_create(semantic_error_expected_compile_time_type, element->source), state->user);
            return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);
        }
        type const left_side_type = object.compile_time_value.value_.type_;
        switch (left_side_type.kind)
        {
        case type_kind_string_ref:
        case type_kind_unit:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_interface:
            state->on_error(semantic_error_create(semantic_error_unknown_element, element->source), state->user);
            return read_structure_element_result_create(false, type_from_unit(), optional_value_empty, false);

        case type_kind_structure:
        case type_kind_function_pointer:
        case type_kind_tuple:
        case type_kind_lambda:
        case type_kind_enum_constructor:
        case type_kind_method_pointer:
        case type_kind_generic_enum:
            LPG_TO_DO();

        case type_kind_enumeration:
        {
            enumeration const *const enum_ = state->program->enums + left_side_type.enum_;
            restore(previous_code);
            LPG_FOR(enum_element_id, i, enum_->size)
            {
                if (unicode_view_equals(
                        unicode_view_from_string(element->value), unicode_view_from_string(enum_->elements[i].name)))
                {
                    if (enum_->elements[i].state.kind == type_kind_unit)
                    {
                        value const literal = value_from_enum_element(i, enum_->elements[i].state, NULL);
                        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                      result, literal, type_from_enumeration(left_side_type.enum_))));
                        return read_structure_element_result_create(
                            true, left_side_type, optional_value_create(literal), true);
                    }
                    value const literal = value_from_enum_constructor();
                    enum_constructor_type *const constructor_type =
                        garbage_collector_allocate(&state->program->memory, sizeof(*constructor_type));
                    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                                  result, literal, type_from_enum_constructor(constructor_type))));
                    constructor_type->enumeration = left_side_type.enum_;
                    constructor_type->which = i;
                    return read_structure_element_result_create(
                        true, type_from_enum_constructor(constructor_type), optional_value_create(literal), true);
                }
            }
            state->on_error(semantic_error_create(semantic_error_unknown_element, element->source), state->user);
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
    case type_kind_lambda:
        return all_functions[callee.lambda.lambda].signature->parameters.length;

    case type_kind_function_pointer:
        return callee.function_pointer_->parameters.length;

    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
        LPG_TO_DO();

    case type_kind_enum_constructor:
    {
        enumeration const *const enum_ = all_enums + callee.enum_constructor->enumeration;
        ASSUME(callee.enum_constructor->which < enum_->size);
        return enum_->elements[callee.enum_constructor->which].state.kind != type_kind_unit;
    }
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result read_variable(LPG_NON_NULL(function_checking_state *const state),
                                                LPG_NON_NULL(instruction_sequence *const to), unicode_view const name,
                                                source_location const where)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, to);

    read_local_variable_result const local = read_local_variable(state, to, name, where);
    switch (local.status)
    {
    case read_local_variable_status_ok:
        if (local.where.captured_in_current_lambda.has_value)
        {
            register_id const captures = allocate_register(&state->used_registers);
            add_instruction(to, instruction_create_get_captures(captures));
            register_id const capture_result = allocate_register(&state->used_registers);
            add_instruction(to, instruction_create_read_struct(read_struct_instruction_create(
                                    captures, local.where.captured_in_current_lambda.value, capture_result)));
            return evaluate_expression_result_create(
                true, capture_result, local.what, local.compile_time_value, local.is_pure, false);
        }
        return evaluate_expression_result_create(
            true, local.where.local_address, local.what, local.compile_time_value, local.is_pure, false);

    case read_local_variable_status_unknown:
        break;

    case read_local_variable_status_forbidden:
        return evaluate_expression_result_empty;
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
        true, result, element_read.type_, element_read.compile_time_value, true, false);
}

static evaluate_expression_result make_unit(register_id *const used_registers, instruction_sequence *output)
{
    type const unit_type = {type_kind_unit, {0}};
    evaluate_expression_result const final_result = evaluate_expression_result_create(
        true, allocate_register(used_registers), unit_type, optional_value_create(value_from_unit()), true, false);
    add_instruction(output, instruction_create_literal(
                                literal_instruction_create(final_result.where, value_from_unit(), type_from_unit())));
    return final_result;
}

static void define_register_debug_name(function_checking_state *const state, register_id const which,
                                       unicode_string const name)
{
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

check_function_result check_function(program_check *const root, function_checking_state *parent,
                                     expression const body_in, structure const global, check_error_handler *on_error,
                                     void *user, checked_program *const program, type const *const parameter_types,
                                     unicode_string *const parameter_names, size_t const parameter_count,
                                     optional_type const self, bool const may_capture_runtime_variables,
                                     optional_type const explicit_return_type)
{
    ASSUME(root);
    instruction_sequence body_out = instruction_sequence_create(NULL, 0);
    function_checking_state state =
        function_checking_state_create(root, parent, may_capture_runtime_variables, &global, on_error, user, program,
                                       &body_out, explicit_return_type, explicit_return_type.is_set);

    if (self.is_set)
    {
        register_id const address = allocate_register(&state.used_registers);
        add_local_variable(
            &state.local_variables, local_variable_create(unicode_view_copy(unicode_view_from_c_str("self")),
                                                          self.value, optional_value_empty, address));
        define_register_debug_name(&state, address, unicode_view_copy(unicode_view_from_c_str("self")));
    }

    for (size_t i = 0; i < parameter_count; ++i)
    {
        register_id const address = allocate_register(&state.used_registers);
        add_local_variable(&state.local_variables,
                           local_variable_create(unicode_view_copy(unicode_view_from_string(parameter_names[i])),
                                                 parameter_types[i], optional_value_empty, address));
        define_register_debug_name(&state, address, unicode_view_copy(unicode_view_from_string(parameter_names[i])));
    }

    evaluate_expression_result const body_evaluated = evaluate_expression(&state, &body_out, body_in);

    for (size_t i = 0; i < state.local_variables.count; ++i)
    {
        local_variable *const variable = (state.local_variables.elements + i);
        local_variable_free(variable);
    }
    if (state.local_variables.elements)
    {
        deallocate(state.local_variables.elements);
    }
    if (state.register_compile_time_values)
    {
        deallocate(state.register_compile_time_values);
        state.register_compile_time_values = NULL;
        state.register_compile_time_value_count = 0;
    }

    type return_type = type_from_unit();
    if (body_evaluated.has_value)
    {
        return_type = body_evaluated.type_;
        if (explicit_return_type.is_set)
        {
            if (body_evaluated.always_returns)
            {
                if (!state.return_type.is_set)
                {
                    LPG_TO_DO();
                }
                return_type = state.return_type.value;
            }
            else
            {
                conversion_result const converted = convert(
                    &state, &body_out, body_evaluated.where, body_evaluated.type_, body_evaluated.compile_time_value,
                    expression_source_begin(body_in), explicit_return_type.value, false);
                switch (converted.ok)
                {
                case failure:
                    for (size_t i = 0; i < state.register_debug_name_count; ++i)
                    {
                        unicode_string_free(state.register_debug_names + i);
                    }
                    if (state.register_debug_names)
                    {
                        deallocate(state.register_debug_names);
                    }
                    instruction_sequence_free(&body_out);
                    return check_function_result_empty;

                case success:
                    return_type = converted.result_type;
                    ASSUME(type_equals(explicit_return_type.value, converted.result_type));
                    break;
                }
                register_id const unit_goes_into = allocate_register(&state.used_registers);
                add_instruction(
                    &body_out, instruction_create_return(return_instruction_create(converted.where, unit_goes_into)));
            }
        }
        else
        {
            if (body_evaluated.always_returns)
            {
                if (!state.return_type.is_set)
                {
                    LPG_TO_DO();
                }
                return_type = state.return_type.value;
            }
            else
            {
                if (body_evaluated.has_value)
                {
                    register_id const unit_goes_into = allocate_register(&state.used_registers);
                    add_instruction(&body_out, instruction_create_return(
                                                   return_instruction_create(body_evaluated.where, unit_goes_into)));
                }
                else if (body_evaluated.compile_time_value.is_set)
                {
                    register_id const return_value = allocate_register(&state.used_registers);
                    add_instruction(
                        &body_out, instruction_create_literal(literal_instruction_create(
                                       return_value, body_evaluated.compile_time_value.value_, body_evaluated.type_)));
                    register_id const unit_goes_into = allocate_register(&state.used_registers);
                    add_instruction(
                        &body_out, instruction_create_return(return_instruction_create(return_value, unit_goes_into)));
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
                    return check_function_result_empty;
                }
            }
        }
    }

    type *const capture_types =
        state.capture_count ? allocate_array(state.capture_count, sizeof(*capture_types)) : NULL;
    for (size_t i = 0; i < state.capture_count; ++i)
    {
        capture_types[i] = state.captures[i].what;
    }

    function_pointer *const signature = allocate(sizeof(*signature));
    *signature =
        function_pointer_create(return_type, tuple_type_create(NULL, 0),
                                tuple_type_create(capture_types, state.capture_count), optional_type_create_empty());

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
    return check_function_result_create(result, state.captures, state.capture_count);
}

static evaluate_expression_result make_compile_time_unit(void)
{
    evaluate_expression_result const result = {
        false, 0, type_from_unit(), optional_value_create(value_from_unit()), true, false};
    return result;
}

typedef struct compile_time_type_expression_result
{
    bool has_value;
    register_id where;
    type compile_time_value;
} compile_time_type_expression_result;

static compile_time_type_expression_result compile_time_type_expression_result_create(bool has_value, register_id where,
                                                                                      type compile_time_value)
{
    compile_time_type_expression_result const result = {has_value, where, compile_time_value};
    return result;
}

static compile_time_type_expression_result
expect_compile_time_type(function_checking_state *state, instruction_sequence *function, expression const element)
{
    evaluate_expression_result const result = evaluate_expression(state, function, element);
    if (result.has_value)
    {
        if (!result.compile_time_value.is_set || (result.compile_time_value.value_.kind != value_kind_type))
        {
            state->on_error(
                semantic_error_create(semantic_error_expected_compile_time_type, expression_source_begin(element)),
                state->user);
            return compile_time_type_expression_result_create(false, ~(register_id)0, type_from_unit());
        }
    }
    return compile_time_type_expression_result_create(
        result.has_value, result.where, result.compile_time_value.value_.type_);
}

typedef struct evaluated_function_header
{
    bool is_success;
    type *parameter_types;
    unicode_string *parameter_names;
    optional_type return_type;
} evaluated_function_header;

static evaluated_function_header
evaluated_function_header_create(type *parameter_types, unicode_string *parameter_names, optional_type return_type)
{
    evaluated_function_header const result = {true, parameter_types, parameter_names, return_type};
    return result;
}

static evaluated_function_header const evaluated_function_header_failure = {
    false, NULL, NULL, {false, {type_kind_unit, {0}}}};

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
        *dummy_signature = function_pointer_create(
            type_from_unit(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());
        state->program->functions[this_lambda_id] =
            checked_function_create(dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    return this_lambda_id;
}

static evaluate_expression_result evaluate_lambda(function_checking_state *const state,
                                                  instruction_sequence *const function, lambda const evaluated)
{
    evaluated_function_header const header = evaluate_function_header(state, function, evaluated.header);
    if (!header.is_success)
    {
        return evaluate_expression_result_empty;
    }
    function_id const this_lambda_id = reserve_function_id(state);
    check_function_result const checked =
        check_function(state->root, state, *evaluated.result, *state->global, state->on_error, state->user,
                       state->program, header.parameter_types, header.parameter_names, evaluated.header.parameter_count,
                       optional_type_create_empty(), true, header.return_type);
    for (size_t i = 0; i < evaluated.header.parameter_count; ++i)
    {
        unicode_string_free(header.parameter_names + i);
    }
    deallocate(header.parameter_names);
    if (!checked.success)
    {
        deallocate(header.parameter_types);
        return evaluate_expression_result_empty;
    }

    ASSUME(checked.function.signature->parameters.length == 0);
    checked.function.signature->parameters.elements = header.parameter_types;
    checked.function.signature->parameters.length = evaluated.header.parameter_count;

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
        deallocate(checked.captures);
        add_instruction(function, instruction_create_lambda_with_captures(lambda_with_captures_instruction_create(
                                      destination, this_lambda_id, captures, checked.capture_count)));
        return evaluate_expression_result_create(true, destination, result_type, optional_value_empty, false, false);
    }
    value const function_pointer =
        value_from_function_pointer(function_pointer_value_from_internal(this_lambda_id, NULL, 0));
    add_instruction(
        function, instruction_create_literal(literal_instruction_create(destination, function_pointer, result_type)));
    return evaluate_expression_result_create(
        true, destination, result_type, optional_value_create(function_pointer), false, false);
}

static optional_size find_implementation(interface const *const in, type const self)
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

static conversion_result convert_to_interface(function_checking_state *const state,
                                              instruction_sequence *const function, register_id const original,
                                              type const from, source_location const original_source,
                                              interface_id const to)
{
    optional_size const impl = find_implementation(state->program->interfaces + to, from);
    if (impl.state == optional_empty)
    {
        state->on_error(semantic_error_create(semantic_error_type_mismatch, original_source), state->user);
        conversion_result const result = {failure, original, from, optional_value_empty};
        return result;
    }
    register_id const converted = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_erase_type(erase_type_instruction_create(
                                  original, converted, implementation_ref_create(to, impl.value_if_set))));
    conversion_result const result = {success, converted, type_from_interface(to), optional_value_empty};
    return result;
}

static conversion_result convert(function_checking_state *const state, instruction_sequence *const function,
                                 register_id const original, type const from,
                                 optional_value const original_compile_time_value,
                                 source_location const original_source, type const to, bool const may_widen_result_type)
{
    if (type_equals(from, to))
    {
        conversion_result const result = {success, original, to, original_compile_time_value};
        return result;
    }
    switch (to.kind)
    {
    case type_kind_enum_constructor:
    case type_kind_enumeration:
    case type_kind_function_pointer:
    case type_kind_lambda:
    case type_kind_string_ref:
    case type_kind_unit:
    case type_kind_structure:
    case type_kind_type:
    {
        state->on_error(semantic_error_create(semantic_error_type_mismatch, original_source), state->user);
        conversion_result const result = {failure, original, from, optional_value_empty};
        return result;
    }

    case type_kind_method_pointer:
    case type_kind_tuple:
    case type_kind_generic_enum:
        LPG_TO_DO();

    case type_kind_integer_range:
    {
        if (from.kind != type_kind_integer_range)
        {
            state->on_error(semantic_error_create(semantic_error_type_mismatch, original_source), state->user);
            conversion_result const result = {failure, original, from, optional_value_empty};
            return result;
        }
        if (integer_less_or_equals(to.integer_range_.minimum, from.integer_range_.minimum) &&
            integer_less_or_equals(from.integer_range_.maximum, to.integer_range_.maximum))
        {
            conversion_result const result = {success, original, to, original_compile_time_value};
            return result;
        }
        if (may_widen_result_type)
        {
            conversion_result const result = {success, original, type_from_integer_range(integer_range_combine(
                                                                     from.integer_range_, to.integer_range_)),
                                              original_compile_time_value};
            return result;
        }
        state->on_error(semantic_error_create(semantic_error_type_mismatch, original_source), state->user);
        conversion_result const result = {failure, original, from, optional_value_empty};
        return result;
    }

    case type_kind_interface:
        return convert_to_interface(state, function, original, from, original_source, to.interface_);
    }
    LPG_UNREACHABLE();
}

typedef struct argument_evaluation_result
{
    success_indicator ok;
    optional_value compile_time_value;
    register_id where;
} argument_evaluation_result;

static argument_evaluation_result evaluate_argument(function_checking_state *const state,
                                                    instruction_sequence *const function,
                                                    expression const argument_tree, type const callee_type,
                                                    size_t const parameter_id)
{
    evaluate_expression_result const argument = evaluate_expression(state, function, argument_tree);
    if (!argument.has_value)
    {
        argument_evaluation_result const result = {failure, optional_value_empty, 0};
        return result;
    }
    conversion_result const converted =
        convert(state, function, argument.where, argument.type_, argument.compile_time_value,
                expression_source_begin(argument_tree),
                get_parameter_type(callee_type, parameter_id, state->program->functions, state->program->enums), false);
    argument_evaluation_result const result = {converted.ok, converted.compile_time_value, converted.where};
    return result;
}

static evaluate_expression_result evaluate_call_expression(function_checking_state *state,
                                                           instruction_sequence *function, call call)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, function);
    evaluate_expression_result const callee = evaluate_expression(state, function, *call.callee);
    if (!callee.has_value)
    {
        return make_compile_time_unit();
    }

    switch (callee.type_.kind)
    {
    case type_kind_lambda:
    case type_kind_function_pointer:
    case type_kind_enum_constructor:
        break;

    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
        state->on_error(
            semantic_error_create(semantic_error_not_callable, expression_source_begin(*call.callee)), state->user);
        return make_compile_time_unit();
    }
    size_t const expected_arguments =
        expected_call_argument_count(callee.type_, state->program->functions, state->program->enums);
    register_id *const arguments = allocate_array(expected_arguments, sizeof(*arguments));
    value *const compile_time_arguments = allocate_array(expected_arguments, sizeof(*compile_time_arguments));
    bool all_compile_time_arguments = true;
    LPG_FOR(size_t, i, call.arguments.length)
    {
        expression const argument_tree = call.arguments.elements[i];
        if (i >= expected_arguments)
        {
            state->on_error(
                semantic_error_create(semantic_error_extraneous_argument, expression_source_begin(argument_tree)),
                state->user);
            break;
        }
        argument_evaluation_result const result = evaluate_argument(state, function, argument_tree, callee.type_, i);
        if (result.ok != success)
        {
            restore(previous_code);
            deallocate(compile_time_arguments);
            deallocate(arguments);
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
    if (call.arguments.length < expected_arguments)
    {
        deallocate(compile_time_arguments);
        restore(previous_code);
        deallocate(arguments);
        state->on_error(semantic_error_create(semantic_error_missing_argument, call.closing_parenthesis), state->user);
        return evaluate_expression_result_empty;
    }
    type const return_type = get_return_type(callee.type_, state->program->functions, state->program->interfaces);
    register_id result = ~(register_id)0;
    optional_value compile_time_result = {false, value_from_unit()};
    if (callee.compile_time_value.is_set && all_compile_time_arguments)
    {
        {
            switch (callee.compile_time_value.value_.kind)
            {
            case value_kind_function_pointer:
            {
                compile_time_result =
                    call_function(callee.compile_time_value.value_.function_pointer,
                                  function_call_arguments_create(
                                      optional_value_empty, compile_time_arguments, state->root->globals,
                                      &state->program->memory, state->program->functions, state->program->interfaces));
                break;
            }

            case value_kind_type_erased:
            case value_kind_integer:
            case value_kind_string:
            case value_kind_flat_object:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
            case value_kind_pattern:
            case value_kind_generic_enum:
                LPG_UNREACHABLE();

            case value_kind_enum_constructor:
            {
                value *const enum_state = garbage_collector_allocate(&state->program->memory, sizeof(*enum_state));
                ASSUME(expected_arguments == 1);
                *enum_state = compile_time_arguments[0];
                compile_time_result = optional_value_create(
                    value_from_enum_element(callee.type_.enum_constructor->which,
                                            state->program->enums[callee.type_.enum_constructor->enumeration]
                                                .elements[callee.type_.enum_constructor->which]
                                                .state,
                                            enum_state));
                break;
            }
            }
        }
        if (compile_time_result.is_set)
        {
            deallocate(arguments);
            restore(previous_code);
            result = allocate_register(&state->used_registers);
            if (return_type.kind == type_kind_string_ref)
            {
                size_t const length = compile_time_result.value_.string_ref.length;
                char *const copy = garbage_collector_allocate(&state->program->memory, length);
                memcpy(copy, compile_time_result.value_.string_ref.begin, length);
                add_instruction(function, instruction_create_literal(literal_instruction_create(
                                              result, value_from_string_ref(unicode_view_create(copy, length)),
                                              type_from_string_ref())));
            }
            else
            {
                add_instruction(function, instruction_create_literal(literal_instruction_create(
                                              result, compile_time_result.value_, return_type)));
            }
        }
    }

    if (!compile_time_result.is_set)
    {
        result = allocate_register(&state->used_registers);
        switch (callee.type_.kind)
        {
        case type_kind_lambda:
        case type_kind_function_pointer:
            add_instruction(function, instruction_create_call(call_instruction_create(
                                          callee.where, arguments, expected_arguments, result)));
            break;

        case type_kind_structure:
        case type_kind_unit:
        case type_kind_string_ref:
        case type_kind_enumeration:
        case type_kind_tuple:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_generic_enum:
            LPG_UNREACHABLE();

        case type_kind_enum_constructor:
            ASSUME(call.arguments.length == 1);
            add_instruction(function, instruction_create_enum_construct(enum_construct_instruction_create(
                                          result, *callee.type_.enum_constructor, arguments[0],
                                          state->program->enums[callee.type_.enum_constructor->enumeration]
                                              .elements[callee.type_.enum_constructor->which]
                                              .state)));
            deallocate(arguments);
            break;

        case type_kind_method_pointer:
        case type_kind_interface:
            LPG_TO_DO();
        }
    }

    deallocate(compile_time_arguments);
    return evaluate_expression_result_create(true, result, return_type, compile_time_result, false, false);
}

evaluate_expression_result evaluate_not_expression(function_checking_state *state, instruction_sequence *function,
                                                   const expression *element)
{
    evaluate_expression_result const result = evaluate_expression(state, function, *(*element).not.expr);
    ASSUME(state->global->members[3].compile_time_value.is_set);

    value boolean_value = state->global->members[3].compile_time_value.value_;

    ASSUME(boolean_value.kind == value_kind_type);
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
            true, result_register, result.type_, optional_value_empty, false, false);
    }
    state->on_error(
        semantic_error_create(semantic_error_type_mismatch, expression_source_begin((*element))), state->user);
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

static void deallocate_integer_range_list_cases(match_instruction_case *cases, size_t const case_count,
                                                integer_range_list integer_ranges)
{
    for (size_t j = 0; j < case_count; ++j)
    {
        match_instruction_case_free(cases[j]);
    }
    if (cases)
    {
        deallocate(cases);
    }
    integer_range_list_deallocate(integer_ranges);
}

typedef enum pattern_evaluate_result_kind
{
    pattern_evaluate_result_kind_is_pattern,
    pattern_evaluate_result_kind_no_pattern,
    pattern_evaluate_result_kind_failure
} pattern_evaluate_result_kind;

typedef struct pattern_evaluate_result
{
    pattern_evaluate_result_kind kind;
    enum_constructor_type stateful_enum_element;
    unicode_view placeholder_name;
    source_location placeholder_name_source;
} pattern_evaluate_result;

static pattern_evaluate_result const pattern_evaluate_result_no = {
    pattern_evaluate_result_kind_no_pattern, {~(enum_id)0, ~(enum_element_id)0}, {NULL, 0}, {0, 0}};

static pattern_evaluate_result const pattern_evaluate_result_failure = {
    pattern_evaluate_result_kind_failure, {~(enum_id)0, ~(enum_element_id)0}, {NULL, 0}, {0, 0}};

static pattern_evaluate_result pattern_evaluate_result_create_pattern(enum_constructor_type stateful_enum_element,
                                                                      unicode_view placeholder_name,
                                                                      source_location placeholder_name_source)
{
    pattern_evaluate_result const result = {
        pattern_evaluate_result_kind_is_pattern, stateful_enum_element, placeholder_name, placeholder_name_source};
    return result;
}

static pattern_evaluate_result check_for_pattern(function_checking_state *state, instruction_sequence *function,
                                                 expression const root)
{
    switch (root.type)
    {
    case expression_type_import:
        LPG_TO_DO();

    case expression_type_generic_instantiation:
        LPG_TO_DO();

    case expression_type_call:
    {
        if ((root.call.arguments.length != 1) || (root.call.arguments.elements[0].type != expression_type_placeholder))
        {
            return pattern_evaluate_result_no;
        }
        evaluate_expression_result const enum_element_evaluated =
            evaluate_expression(state, function, *root.call.callee);
        if (!enum_element_evaluated.has_value)
        {
            return pattern_evaluate_result_failure;
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
    case expression_type_tuple:
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
                                                        size_t const previous_number_of_variables)
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
        final_result = evaluate_expression(state, output, input.elements[i]);
        if (!final_result.is_pure)
        {
            is_pure = false;
        }
        if (final_result.always_returns)
        {
            always_returns = true;
        }
    }
    for (size_t i = previous_number_of_variables, c = state->local_variables.count; i != c; ++i)
    {
        local_variable_free(state->local_variables.elements + i);
    }
    state->local_variables.count = previous_number_of_variables;
    return evaluate_expression_result_create(final_result.has_value, final_result.where, final_result.type_,
                                             final_result.compile_time_value, is_pure, always_returns);
}

evaluate_expression_result evaluate_match_expression(function_checking_state *state, instruction_sequence *function,
                                                     const expression *element)
{
    instruction_checkpoint const before = make_checkpoint(state, function);
    evaluate_expression_result const key = evaluate_expression(state, function, *(*element).match.input);
    if (!key.has_value)
    {
        return key;
    }
    switch (key.type_.kind)
    {
    case type_kind_enumeration:
    {
        enumeration const *const enum_ = state->program->enums + key.type_.enum_;
        if (enum_->size == 0)
        {
            state->on_error(
                semantic_error_create(semantic_error_match_unsupported, expression_source_begin((*element))),
                state->user);
            return evaluate_expression_result_empty;
        }

        if (enum_->size != (*element).match.number_of_cases)
        {
            state->on_error(
                semantic_error_create(semantic_error_missing_match_case, expression_source_begin((*element))),
                state->user);
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
            pattern_evaluate_result const maybe_pattern = check_for_pattern(state, function, *case_tree.key);
            switch (maybe_pattern.kind)
            {
            case pattern_evaluate_result_kind_is_pattern:
            {
                if (maybe_pattern.stateful_enum_element.enumeration != key.type_.enum_)
                {
                    state->on_error(
                        semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.key)),
                        state->user);
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return evaluate_expression_result_empty;
                }

                {
                    bool *const case_handled = (enum_elements_handled + maybe_pattern.stateful_enum_element.which);
                    if (*case_handled)
                    {
                        state->on_error(semantic_error_create(semantic_error_duplicate_match_case,
                                                              expression_source_begin(*case_tree.key)),
                                        state->user);
                        deallocate_boolean_cases(cases, enum_elements_handled, i);
                        return evaluate_expression_result_empty;
                    }
                    *case_handled = true;
                }

                size_t const previous_number_of_variables = state->local_variables.count;

                placeholder_where = allocate_register(&state->used_registers);
                if (local_variable_name_exists(state->local_variables, maybe_pattern.placeholder_name))
                {
                    state->on_error(semantic_error_create(semantic_error_declaration_with_existing_name,
                                                          maybe_pattern.placeholder_name_source),
                                    state->user);
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return evaluate_expression_result_empty;
                }

                ASSUME(placeholder_where != ~(register_id)0);
                add_local_variable(
                    &state->local_variables,
                    local_variable_create(unicode_view_copy(maybe_pattern.placeholder_name),
                                          enum_->elements[maybe_pattern.stateful_enum_element.which].state,
                                          optional_value_empty, placeholder_where));
                define_register_debug_name(state, placeholder_where, unicode_view_copy(maybe_pattern.placeholder_name));

                action_evaluated = check_sequence_finish(
                    state, &action, sequence_create(case_tree.action, 1), previous_number_of_variables);
                break;
            }

            case pattern_evaluate_result_kind_no_pattern:
            {
                evaluate_expression_result const key_evaluated = evaluate_expression(state, function, *case_tree.key);
                if (!key_evaluated.has_value)
                {
                    deallocate_boolean_cases(cases, enum_elements_handled, i);
                    return key_evaluated;
                }

                if (!type_equals(key.type_, key_evaluated.type_))
                {
                    state->on_error(
                        semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.key)),
                        state->user);
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
                        state->on_error(semantic_error_create(semantic_error_duplicate_match_case,
                                                              expression_source_begin(*case_tree.key)),
                                        state->user);
                        deallocate_boolean_cases(cases, enum_elements_handled, i);
                        return evaluate_expression_result_empty;
                    }
                    *case_handled = true;
                }

                /*TODO: support runtime values as keys?*/
                ASSERT(key_evaluated.compile_time_value.is_set);

                is_always_this_case =
                    key.compile_time_value.is_set && key.is_pure &&
                    value_equals(key.compile_time_value.value_, key_evaluated.compile_time_value.value_);
                key_value = key_evaluated.where;

                action_evaluated = evaluate_expression(state, &action, *case_tree.action);
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

            if (!action_evaluated.has_value)
            {
                deallocate_boolean_cases(cases, enum_elements_handled, i);
                instruction_sequence_free(&action);
                return action_evaluated;
            }

            if (i == 0)
            {
                result_type = action_evaluated.type_;
            }
            else if (!type_equals(result_type, action_evaluated.type_))
            {
                /*TODO: support types that are not the same, but still comparable*/
                state->on_error(
                    semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.action)),
                    state->user);
                deallocate_boolean_cases(cases, enum_elements_handled, i);
                instruction_sequence_free(&action);
                return evaluate_expression_result_empty;
            }

            if (!compile_time_result.is_set && is_always_this_case && action_evaluated.compile_time_value.is_set &&
                action_evaluated.is_pure)
            {
                compile_time_result = optional_value_create(action_evaluated.compile_time_value.value_);
            }

            all_cases_return = (all_cases_return && action_evaluated.always_returns);

            switch (maybe_pattern.kind)
            {
            case pattern_evaluate_result_kind_is_pattern:
                ASSUME(placeholder_where != ~(register_id)0);
                cases[i] = match_instruction_case_create_stateful_enum(
                    match_instruction_case_stateful_enum_create(
                        maybe_pattern.stateful_enum_element.which, placeholder_where),
                    action, action_evaluated.where);
                break;

            case pattern_evaluate_result_kind_no_pattern:
                ASSUME(key_value != ~(register_id)0);
                cases[i] = match_instruction_case_create_value(key_value, action, action_evaluated.where);
                break;

            case pattern_evaluate_result_kind_failure:
                LPG_UNREACHABLE();
            }
        }
        for (size_t i = 0; i < (*element).match.number_of_cases; ++i)
        {
            ASSUME(enum_elements_handled[i]);
        }
        register_id result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_match(match_instruction_create(
                          key.where, cases, (*element).match.number_of_cases, result_register, result_type)));
        deallocate(enum_elements_handled);
        if (compile_time_result.is_set)
        {
            restore(before);
            result_register = allocate_register(&state->used_registers);
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          result_register, compile_time_result.value_, result_type)));
            return evaluate_expression_result_create(
                true, result_register, result_type, compile_time_result, true, all_cases_return);
        }

        return evaluate_expression_result_create(
            true, result_register, result_type, optional_value_empty, false, all_cases_return);
    }

    case type_kind_integer_range:
    {
        integer const expected_cases = integer_range_size(key.type_.integer_range_);

        if (!integer_equal(expected_cases, integer_create(0, (*element).match.number_of_cases)))
        {
            state->on_error(
                semantic_error_create(semantic_error_missing_match_case, expression_source_begin((*element))),
                state->user);
            return evaluate_expression_result_empty;
        }

        match_instruction_case *const cases = allocate_array((*element).match.number_of_cases, sizeof(*cases));

        integer_range_list integer_ranges_unhandled =
            integer_range_list_create_from_integer_range(key.type_.integer_range_);

        type result_type = type_from_unit();
        optional_value compile_time_result = optional_value_empty;

        bool all_cases_return = true;
        for (size_t i = 0; i < (*element).match.number_of_cases; ++i)
        {
            match_case const case_tree = (*element).match.cases[i];
            evaluate_expression_result const key_evaluated = evaluate_expression(state, function, *case_tree.key);
            if (!key_evaluated.has_value)
            {
                deallocate_integer_range_list_cases(cases, i, integer_ranges_unhandled);
                return key_evaluated;
            }

            if (key_evaluated.type_.kind != type_kind_integer_range)
            {
                state->on_error(
                    semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.key)),
                    state->user);
                deallocate_integer_range_list_cases(cases, i, integer_ranges_unhandled);
                return evaluate_expression_result_empty;
            }

            {
                if (integer_range_list_contains(integer_ranges_unhandled, key_evaluated.type_.integer_range_))
                {
                    integer_range_list_remove(&integer_ranges_unhandled, key_evaluated.type_.integer_range_);
                }
                else
                {
                    // TODO: Better error description
                    state->on_error(semantic_error_create(
                                        semantic_error_duplicate_match_case, expression_source_begin(*case_tree.key)),
                                    state->user);
                    deallocate_integer_range_list_cases(cases, i, integer_ranges_unhandled);
                    return evaluate_expression_result_empty;
                }
            }

            /*TODO: support runtime values as keys?*/
            ASSERT(key_evaluated.compile_time_value.is_set);

            instruction_sequence action = instruction_sequence_create(NULL, 0);
            evaluate_expression_result const action_evaluated = evaluate_expression(state, &action, *case_tree.action);
            if (!action_evaluated.has_value)
            {
                deallocate_integer_range_list_cases(cases, i, integer_ranges_unhandled);
                instruction_sequence_free(&action);
                return action_evaluated;
            }

            all_cases_return = (all_cases_return && action_evaluated.always_returns);

            if (i == 0)
            {
                result_type = action_evaluated.type_;
            }
            else if (!type_equals(result_type, action_evaluated.type_))
            {
                /*TODO: support types that are not the same, but still comparable*/
                state->on_error(
                    semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*case_tree.action)),
                    state->user);
                deallocate_integer_range_list_cases(cases, i, integer_ranges_unhandled);
                instruction_sequence_free(&action);
                return evaluate_expression_result_empty;
            }

            if (!compile_time_result.is_set && key.compile_time_value.is_set && key.is_pure &&
                value_equals(key.compile_time_value.value_, key_evaluated.compile_time_value.value_) &&
                action_evaluated.compile_time_value.is_set && action_evaluated.is_pure)
            {
                compile_time_result = optional_value_create(action_evaluated.compile_time_value.value_);
            }

            cases[i] = match_instruction_case_create_value(key_evaluated.where, action, action_evaluated.where);
        }

        ASSUME(integer_equal(integer_range_list_size(integer_ranges_unhandled), integer_create(0, 0)));
        if (compile_time_result.is_set)
        {
            deallocate_integer_range_list_cases(cases, (*element).match.number_of_cases, integer_ranges_unhandled);
            restore(before);
            register_id const result_register = allocate_register(&state->used_registers);
            add_instruction(function, instruction_create_literal(literal_instruction_create(
                                          result_register, compile_time_result.value_, result_type)));
            return evaluate_expression_result_create(
                true, result_register, result_type, compile_time_result, true, all_cases_return);
        }
        integer_range_list_deallocate(integer_ranges_unhandled);
        register_id const result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_match(match_instruction_create(
                          key.where, cases, (*element).match.number_of_cases, result_register, result_type)));
        return evaluate_expression_result_create(
            true, result_register, result_type, optional_value_empty, false, all_cases_return);
    }

    case type_kind_method_pointer:
    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_lambda:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_interface:
    case type_kind_generic_enum:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

typedef struct evaluate_arguments_result
{
    register_id *registers;
    tuple_type tuple_type_for_instruction;
    tuple_type tuple_type_for_result;
} evaluate_arguments_result;

static evaluate_arguments_result evaluate_arguments(function_checking_state *const state,
                                                    instruction_sequence *const function,
                                                    expression const *const arguments, size_t const argument_count)
{
    register_id *registers = allocate_array(argument_count, sizeof(*registers));
    tuple_type const tuple_type_for_instruction = {
        allocate_array(argument_count, sizeof(*tuple_type_for_instruction.elements)), argument_count};
    tuple_type const tuple_type_for_result = {
        garbage_collector_allocate_array(
            &state->program->memory, argument_count, sizeof(*tuple_type_for_instruction.elements)),
        argument_count};
    for (size_t i = 0; i < argument_count; ++i)
    {
        evaluate_expression_result const argument_evaluated = evaluate_expression(state, function, arguments[i]);
        if (!argument_evaluated.has_value)
        {
            deallocate(registers);
            deallocate(tuple_type_for_instruction.elements);
            evaluate_arguments_result const result = {NULL, tuple_type_create(NULL, 0), tuple_type_create(NULL, 0)};
            return result;
        }
        registers[i] = argument_evaluated.where;
        tuple_type_for_instruction.elements[i] = argument_evaluated.type_;
        tuple_type_for_result.elements[i] = argument_evaluated.type_;
    }
    evaluate_arguments_result const result = {registers, tuple_type_for_instruction, tuple_type_for_result};
    return result;
}

evaluate_expression_result evaluate_tuple_expression(function_checking_state *state, instruction_sequence *function,
                                                     const tuple element)
{
    evaluate_arguments_result const arguments = evaluate_arguments(state, function, element.elements, element.length);
    if (!arguments.registers)
    {
        return evaluate_expression_result_empty;
    }
    register_id const result_register = allocate_register(&state->used_registers);
    add_instruction(
        function, instruction_create_tuple(tuple_instruction_create(
                      arguments.registers, element.length, result_register, arguments.tuple_type_for_instruction)));
    return evaluate_expression_result_create(true, result_register,
                                             type_from_tuple_type(arguments.tuple_type_for_result),
                                             optional_value_empty, false, false);
}

static evaluate_expression_result evaluate_interface(function_checking_state *state, instruction_sequence *function,
                                                     interface_expression const element)
{
    method_description *const methods = allocate_array(element.method_count, sizeof(*methods));
    for (size_t i = 0; i < element.method_count; ++i)
    {
        interface_expression_method const method = element.methods[i];
        evaluated_function_header const header = evaluate_function_header(state, function, method.header);
        if (!header.is_success)
        {
            for (size_t k = 0; k < i; ++k)
            {
                method_description_free(methods[k]);
            }
            deallocate(methods);
            return evaluate_expression_result_empty;
        }
        ASSUME(header.return_type.is_set);
        for (size_t j = 0; j < method.header.parameter_count; ++j)
        {
            unicode_string_free(header.parameter_names + j);
        }
        if (header.parameter_names)
        {
            deallocate(header.parameter_names);
        }
        for (size_t k = 0; k < i; ++k)
        {
            if (unicode_view_equals(
                    unicode_view_from_string(methods[k].name), unicode_view_from_string(method.name.value)))
            {
                state->on_error(
                    semantic_error_create(semantic_error_duplicate_method_name, method.name.source), state->user);
            }
        }
        methods[i] = method_description_create(unicode_view_copy(unicode_view_from_string(method.name.value)),
                                               tuple_type_create(header.parameter_types, method.header.parameter_count),
                                               header.return_type.value);
    }
    interface_id const id = state->program->interface_count;
    state->program->interfaces =
        reallocate_array(state->program->interfaces, (id + 1), sizeof(*state->program->interfaces));
    state->program->interfaces[id] =
        interface_create(methods, /*TODO avoid truncation safely*/ (function_id)element.method_count, NULL, 0);
    state->program->interface_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    value const result = value_from_type(type_from_interface(id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, result, type_from_type())));
    return evaluate_expression_result_create(true, into, type_from_type(), optional_value_create(result), false, false);
}

static evaluate_expression_result evaluate_struct(function_checking_state *state, instruction_sequence *function,
                                                  struct_expression const structure)
{
    structure_member *elements = allocate_array(structure.element_count, sizeof(*elements));
    for (size_t i = 0; i < structure.element_count; ++i)
    {
        struct_expression_element const element = structure.elements[i];
        evaluate_expression_result const element_type_evaluated = evaluate_expression(state, function, element.type);
        if (!element_type_evaluated.has_value)
        {
            for (size_t j = 0; j < i; ++j)
            {
                struct_member_free(elements + j);
            }
            deallocate(elements);
            return make_compile_time_unit();
        }
        if (!element_type_evaluated.compile_time_value.is_set ||
            (element_type_evaluated.compile_time_value.value_.kind != value_kind_type))
        {
            state->on_error(
                semantic_error_create(semantic_error_expected_compile_time_type, expression_source_begin(element.type)),
                state->user);
            for (size_t j = 0; j < i; ++j)
            {
                struct_member_free(elements + j);
            }
            deallocate(elements);
            return make_compile_time_unit();
        }
        elements[i] = structure_member_create(element_type_evaluated.compile_time_value.value_.type_,
                                              unicode_view_copy(unicode_view_from_string(element.name.value)),
                                              optional_value_empty);
    }
    struct_id const id = state->program->struct_count;
    state->program->structs = reallocate_array(state->program->structs, (id + 1), sizeof(*state->program->structs));
    state->program->structs[id] =
        structure_create(elements, /*TODO avoid truncation safely*/ (struct_id)structure.element_count);
    state->program->struct_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    value const result = value_from_type(type_from_struct(id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, result, type_from_type())));
    return evaluate_expression_result_create(true, into, type_from_type(), optional_value_create(result), false, false);
}

typedef struct method_evaluation_result
{
    bool is_success;
    function_pointer_value success;
} method_evaluation_result;

static method_evaluation_result const method_evaluation_result_failure = {false, {0, NULL, NULL, NULL, 0}};

static method_evaluation_result method_evaluation_result_create(function_pointer_value success)
{
    method_evaluation_result const result = {true, success};
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
        return method_evaluation_result_failure;
    }
    if (header.return_type.is_set && !is_implicitly_convertible(header.return_type.value, declared_result_type))
    {
        state->on_error(
            semantic_error_create(semantic_error_type_mismatch, expression_source_begin(*method.header.return_type)),
            state->user);
        for (size_t i = 0; i < method.header.parameter_count; ++i)
        {
            unicode_string_free(header.parameter_names + i);
        }
        deallocate(header.parameter_names);
        deallocate(header.parameter_types);
        return method_evaluation_result_failure;
    }
    function_id const this_lambda_id = reserve_function_id(state);
    check_function_result const checked = check_function(
        state->root, state, expression_from_sequence(method.body), *state->global, state->on_error, state->user,
        state->program, header.parameter_types, header.parameter_names, method.header.parameter_count,
        optional_type_create_set(self), false, optional_type_create_set(declared_result_type));
    for (size_t i = 0; i < method.header.parameter_count; ++i)
    {
        unicode_string_free(header.parameter_names + i);
    }
    deallocate(header.parameter_names);
    if (!checked.success)
    {
        deallocate(header.parameter_types);
        return method_evaluation_result_failure;
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

static void add_implementation(interface *const to, implementation_entry const added)
{
    size_t const new_impl_count = to->implementation_count + 1;
    to->implementations = reallocate_array(to->implementations, new_impl_count, sizeof(*to->implementations));
    to->implementations[to->implementation_count] = added;
    to->implementation_count = new_impl_count;
}

static evaluate_expression_result evaluate_impl(function_checking_state *state, instruction_sequence *const function,
                                                impl_expression const element)
{
    instruction_checkpoint const previous_code = make_checkpoint(state, function);

    compile_time_type_expression_result const interface_evaluated =
        expect_compile_time_type(state, function, *element.interface);
    if (!interface_evaluated.has_value)
    {
        return evaluate_expression_result_empty;
    }
    if (interface_evaluated.compile_time_value.kind != type_kind_interface)
    {
        state->on_error(
            semantic_error_create(semantic_error_expected_interface, expression_source_begin(*element.interface)),
            state->user);
        return evaluate_expression_result_empty;
    }
    interface *const target_interface = state->program->interfaces + interface_evaluated.compile_time_value.interface_;

    compile_time_type_expression_result const self_evaluated = expect_compile_time_type(state, function, *element.self);
    if (!self_evaluated.has_value)
    {
        return evaluate_expression_result_empty;
    }

    type const self = self_evaluated.compile_time_value;

    restore(previous_code);
    function_pointer_value *const methods = allocate_array(element.method_count, sizeof(*methods));
    for (size_t i = 0; i < element.method_count; ++i)
    {
        method_evaluation_result const method =
            evaluate_method_definition(state, function, element.methods[i], self, target_interface->methods[i].result);
        if (!method.is_success)
        {
            deallocate(methods);
            return evaluate_expression_result_empty;
        }
        methods[i] = method.success;
    }
    for (size_t i = 0; i < target_interface->implementation_count; ++i)
    {
        if (type_equals(self, target_interface->implementations[i].self))
        {
            state->on_error(
                semantic_error_create(semantic_error_duplicate_impl, expression_source_begin(*element.self)),
                state->user);
            deallocate(methods);
            return evaluate_expression_result_empty;
        }
    }
    add_implementation(
        target_interface, implementation_entry_create(self, implementation_create(methods, element.method_count)));
    return make_unit(&state->used_registers, function);
}

static evaluate_expression_result evaluate_instantiate_struct(function_checking_state *const state,
                                                              instruction_sequence *const function,
                                                              instantiate_struct_expression const element)
{
    evaluate_expression_result const type_evaluated = evaluate_expression(state, function, *element.type);
    if (!type_evaluated.has_value)
    {
        return type_evaluated;
    }
    if (!type_evaluated.compile_time_value.is_set)
    {
        state->on_error(
            semantic_error_create(semantic_error_expected_compile_time_type, expression_source_begin(*element.type)),
            state->user);
        return make_compile_time_unit();
    }
    if (type_evaluated.compile_time_value.value_.kind != value_kind_type)
    {
        state->on_error(
            semantic_error_create(semantic_error_expected_compile_time_type, expression_source_begin(*element.type)),
            state->user);
        return make_compile_time_unit();
    }
    if (type_evaluated.compile_time_value.value_.type_.kind != type_kind_structure)
    {
        state->on_error(
            semantic_error_create(semantic_error_expected_structure, expression_source_begin(*element.type)),
            state->user);
        return make_compile_time_unit();
    }
    struct_id const structure_id = type_evaluated.compile_time_value.value_.type_.structure_;
    ASSUME(structure_id < state->program->struct_count);
    structure const *const structure = state->program->structs + structure_id;
    if (element.arguments.length < structure->count)
    {
        state->on_error(semantic_error_create(semantic_error_missing_argument, expression_source_begin(*element.type)),
                        state->user);
        return make_compile_time_unit();
    }
    if (element.arguments.length > structure->count)
    {
        state->on_error(
            semantic_error_create(semantic_error_extraneous_argument, expression_source_begin(*element.type)),
            state->user);
        return make_compile_time_unit();
    }
    evaluate_arguments_result const arguments_evaluated =
        evaluate_arguments(state, function, element.arguments.elements, element.arguments.length);
    if (!arguments_evaluated.registers)
    {
        return make_compile_time_unit();
    }
    if (arguments_evaluated.tuple_type_for_instruction.elements)
    {
        deallocate(arguments_evaluated.tuple_type_for_instruction.elements);
    }
    value *const compile_time_values = garbage_collector_allocate_array(
        &state->program->memory, arguments_evaluated.tuple_type_for_result.length, sizeof(*compile_time_values));
    bool is_pure = true;
    for (register_id i = 0; i < arguments_evaluated.tuple_type_for_result.length; ++i)
    {
        optional_value const element = read_register_compile_time_value(state, arguments_evaluated.registers[i]);
        if (!element.is_set)
        {
            is_pure = false;
            break;
        }
        compile_time_values[i] = element.value_;
    }
    register_id const into = allocate_register(&state->used_registers);
    if (is_pure)
    {
        deallocate(arguments_evaluated.registers);
        value const compile_time_value = value_from_flat_object(compile_time_values);
        add_instruction(function, instruction_create_literal(literal_instruction_create(
                                      into, compile_time_value, type_from_struct(structure_id))));
        write_register_compile_time_value(state, into, compile_time_value);
        return evaluate_expression_result_create(
            true, into, type_from_struct(structure_id), optional_value_create(compile_time_value), true, false);
    }
    add_instruction(function, instruction_create_instantiate_struct(instantiate_struct_instruction_create(
                                  into, structure_id, arguments_evaluated.registers, element.arguments.length)));
    return evaluate_expression_result_create(
        true, into, type_from_struct(structure_id), optional_value_empty, false, false);
}

static evaluate_expression_result evaluate_type_of(function_checking_state *const state,
                                                   instruction_sequence *const function,
                                                   type_of_expression const element)
{
    instruction_checkpoint const before = make_checkpoint(state, function);
    evaluate_expression_result const target_evaluated = evaluate_expression(state, function, *element.target);
    restore(before);
    register_id const where = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  where, value_from_type(target_evaluated.type_), type_from_type())));
    write_register_compile_time_value(state, where, value_from_type(target_evaluated.type_));
    return evaluate_expression_result_create(
        true, where, type_from_type(), optional_value_create(value_from_type(target_evaluated.type_)), true, false);
}

typedef struct generic_enum_closures
{
    generic_enum_closure *elements;
    size_t count;
} generic_enum_closures;

static void resolve_generic_enum_closure_identifier(generic_enum_closures *const closures,
                                                    function_checking_state *const state, unicode_view const name,
                                                    source_location const source)
{
    for (size_t i = 0; i < state->local_variables.count; ++i)
    {
        if (unicode_view_equals(name, unicode_view_from_string(state->local_variables.elements[i].name)))
        {
            if (!state->local_variables.elements[i].compile_time_value.is_set)
            {
                state->on_error(semantic_error_create(semantic_error_expected_compile_time_type, source), state->user);
                return;
            }
            closures->elements = reallocate_array(closures->elements, closures->count + 1, sizeof(*closures->elements));
            closures->elements[closures->count] =
                generic_enum_closure_create(unicode_view_copy(name), state->local_variables.elements[i].type_,
                                            state->local_variables.elements[i].compile_time_value.value_);
            closures->count += 1;
            return;
        }
    }
    if (state->parent)
    {
        resolve_generic_enum_closure_identifier(closures, state->parent, name, source);
    }
}

static void find_generic_enum_closures_in_expression(generic_enum_closures *const closures,
                                                     function_checking_state *const state, expression const from);

static void find_generic_enum_closures_in_function_header(generic_enum_closures *const closures,
                                                          function_checking_state *const state,
                                                          function_header_tree const header)
{
    for (size_t i = 0; i < header.parameter_count; ++i)
    {
        find_generic_enum_closures_in_expression(closures, state, *header.parameters[i].type);
    }
    if (header.return_type)
    {
        find_generic_enum_closures_in_expression(closures, state, *header.return_type);
    }
}

static void find_generic_enum_closures_in_sequence(generic_enum_closures *const closures,
                                                   function_checking_state *const state, sequence const tree)
{
    for (size_t i = 0; i < tree.length; ++i)
    {
        find_generic_enum_closures_in_expression(closures, state, tree.elements[i]);
    }
}

static void find_generic_enum_closures_in_expression(generic_enum_closures *const closures,
                                                     function_checking_state *const state, expression const from)
{
    switch (from.type)
    {
    case expression_type_import:
        LPG_TO_DO();

    case expression_type_lambda:
        find_generic_enum_closures_in_function_header(closures, state, from.lambda.header);
        find_generic_enum_closures_in_expression(closures, state, *from.lambda.result);
        break;

    case expression_type_type_of:
        LPG_TO_DO();

    case expression_type_call:
        find_generic_enum_closures_in_expression(closures, state, *from.call.callee);
        for (size_t i = 0; i < from.call.arguments.length; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.call.arguments.elements[i]);
        }
        break;

    case expression_type_integer_literal:
        break;

    case expression_type_access_structure:
        find_generic_enum_closures_in_expression(closures, state, *from.access_structure.object);
        break;

    case expression_type_match:
        find_generic_enum_closures_in_expression(closures, state, *from.match.input);
        for (size_t i = 0; i < from.match.number_of_cases; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, *from.match.cases[i].key);
            find_generic_enum_closures_in_expression(closures, state, *from.match.cases[i].action);
        }
        break;

    case expression_type_string:
        break;

    case expression_type_identifier:
        resolve_generic_enum_closure_identifier(
            closures, state, unicode_view_from_string(from.identifier.value), from.identifier.source);
        break;

    case expression_type_not:
        find_generic_enum_closures_in_expression(closures, state, *from.not.expr);
        break;

    case expression_type_binary:
        LPG_TO_DO();

    case expression_type_return:
        find_generic_enum_closures_in_expression(closures, state, *from.return_);
        break;

    case expression_type_loop:
        for (size_t i = 0; i < from.loop_body.length; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.loop_body.elements[i]);
        }
        break;

    case expression_type_break:
        break;

    case expression_type_sequence:
        find_generic_enum_closures_in_sequence(closures, state, from.sequence);
        break;

    case expression_type_declare:
        if (from.declare.optional_type)
        {
            find_generic_enum_closures_in_expression(closures, state, *from.declare.optional_type);
        }
        find_generic_enum_closures_in_expression(closures, state, *from.declare.initializer);
        break;

    case expression_type_tuple:
        for (size_t i = 0; i < from.tuple.length; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.tuple.elements[i]);
        }
        break;

    case expression_type_comment:
        break;

    case expression_type_interface:
        for (size_t i = 0; i < from.interface.method_count; ++i)
        {
            find_generic_enum_closures_in_function_header(closures, state, from.interface.methods[i].header);
        }
        break;

    case expression_type_struct:
        for (size_t i = 0; i < from.struct_.element_count; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.struct_.elements[i].type);
        }
        break;

    case expression_type_impl:
        find_generic_enum_closures_in_expression(closures, state, *from.impl.interface);
        find_generic_enum_closures_in_expression(closures, state, *from.impl.self);
        for (size_t i = 0; i < from.impl.method_count; ++i)
        {
            find_generic_enum_closures_in_function_header(closures, state, from.impl.methods[i].header);
            find_generic_enum_closures_in_sequence(closures, state, from.impl.methods[i].body);
        }
        break;

    case expression_type_instantiate_struct:
        find_generic_enum_closures_in_expression(closures, state, *from.instantiate_struct.type);
        for (size_t i = 0; i < from.instantiate_struct.arguments.length; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.instantiate_struct.arguments.elements[i]);
        }
        break;

    case expression_type_enum:
        for (size_t i = 0; i < from.enum_.element_count; ++i)
        {
            if (from.enum_.elements[i].state)
            {
                find_generic_enum_closures_in_expression(closures, state, *from.enum_.elements[i].state);
            }
        }
        break;

    case expression_type_placeholder:
        break;

    case expression_type_generic_instantiation:
        find_generic_enum_closures_in_expression(closures, state, *from.generic_instantiation.generic);
        for (size_t i = 0; i < from.generic_instantiation.count; ++i)
        {
            find_generic_enum_closures_in_expression(closures, state, from.generic_instantiation.arguments[i]);
        }
        break;
    }
}

static generic_enum_closures find_generic_enum_closures(function_checking_state *state,
                                                        enum_expression const definition)
{
    generic_enum_closures result = {NULL, 0};
    for (size_t i = 0; i < definition.element_count; ++i)
    {
        if (definition.elements[i].state)
        {
            find_generic_enum_closures_in_expression(&result, state, *definition.elements[i].state);
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
    generic_enum_closures const closures = find_generic_enum_closures(state, element);
    root->generic_enums[id] = generic_enum_create(element, closures.elements, closures.count);
    root->generic_enum_count += 1;
    register_id const into = allocate_register(&state->used_registers);
    add_instruction(function, instruction_create_literal(literal_instruction_create(
                                  into, value_from_generic_enum(id), type_from_generic_enum())));
    write_register_compile_time_value(state, into, value_from_generic_enum(id));
    return evaluate_expression_result_create(
        true, into, type_from_generic_enum(), optional_value_create(value_from_generic_enum(id)), true, false);
}

static evaluate_expression_result
evaluate_enum_expression(function_checking_state *state, instruction_sequence *function, enum_expression const element)
{
    if (element.generic_parameter_count > 0)
    {
        return evaluate_generic_enum_expression(state, function, element);
    }
    enum_id const new_enum_id = state->program->enum_count;
    state->program->enum_count += 1;
    state->program->enums = reallocate_array(state->program->enums, (new_enum_id + 1), sizeof(*state->program->enums));
    enumeration_element *const elements = allocate_array(element.element_count, sizeof(*elements));
    for (size_t i = 0; i < element.element_count; ++i)
    {
        type element_state = type_from_unit();
        if (element.elements[i].state)
        {
            evaluate_expression_result const state_evaluated =
                evaluate_expression(state, function, *element.elements[i].state);
            if (state_evaluated.has_value)
            {
                if (!state_evaluated.compile_time_value.is_set ||
                    (state_evaluated.compile_time_value.value_.kind != value_kind_type))
                {
                    state->on_error(semantic_error_create(semantic_error_expected_compile_time_type,
                                                          expression_source_begin(*element.elements[i].state)),
                                    state->user);
                    for (size_t m = 0; m < i; ++m)
                    {
                        enumeration_element_free(elements + m);
                    }
                    deallocate(elements);
                    state->program->enum_count -= 1;
                    return evaluate_expression_result_empty;
                }
                element_state = state_evaluated.compile_time_value.value_.type_;
            }
            else
            {
                for (size_t m = 0; m < i; ++m)
                {
                    enumeration_element_free(elements + m);
                }
                deallocate(elements);
                state->program->enum_count -= 1;
                return evaluate_expression_result_empty;
            }
        }

        elements[i] = enumeration_element_create(
            unicode_view_copy(unicode_view_from_string(element.elements[i].name)), element_state);
        for (size_t k = 0; k < i; ++k)
        {
            if (unicode_string_equals(element.elements[i].name, element.elements[k].name))
            {
                state->on_error(
                    semantic_error_create(semantic_error_duplicate_enum_element, element.begin), state->user);
                for (size_t m = 0; m <= i; ++m)
                {
                    enumeration_element_free(elements + m);
                }
                deallocate(elements);
                state->program->enum_count -= 1;
                return evaluate_expression_result_empty;
            }
        }
    }
    state->program->enums[new_enum_id] = enumeration_create(elements, element.element_count);
    register_id const into = allocate_register(&state->used_registers);
    value const literal = value_from_type(type_from_enumeration(new_enum_id));
    add_instruction(function, instruction_create_literal(literal_instruction_create(into, literal, type_from_type())));
    write_register_compile_time_value(state, into, literal);
    return evaluate_expression_result_create(true, into, type_from_type(), optional_value_create(literal), true, false);
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
            true, into, type_from_type(), optional_value_create(literal), true, false);
    }
    enum_expression const original = root->generic_enums[generic].tree;
    instruction_sequence ignored_instructions = instruction_sequence_create(NULL, 0);
    function_checking_state enum_checking =
        function_checking_state_create(state->root, NULL, false, state->global, state->on_error, state->user,
                                       state->program, &ignored_instructions, optional_type_create_empty(), false);
    generic_enum const instantiated_enum = state->root->generic_enums[generic];
    if (argument_count < instantiated_enum.tree.generic_parameter_count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        state->on_error(semantic_error_create(semantic_error_missing_argument, where), state->user);
        return evaluate_expression_result_empty;
    }
    if (argument_count > instantiated_enum.tree.generic_parameter_count)
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        state->on_error(semantic_error_create(semantic_error_extraneous_argument, where), state->user);
        return evaluate_expression_result_empty;
    }
    for (size_t i = 0; i < instantiated_enum.tree.generic_parameter_count; ++i)
    {
        register_id const argument_register = allocate_register(&enum_checking.used_registers);
        add_local_variable(
            &enum_checking.local_variables,
            local_variable_create(
                unicode_view_copy(unicode_view_from_string(instantiated_enum.tree.generic_parameters[i])),
                argument_types[i], optional_value_create(arguments[i]), argument_register));
    }
    for (size_t i = 0; i < instantiated_enum.closure_count; ++i)
    {
        register_id const closure_register = allocate_register(&enum_checking.used_registers);
        add_local_variable(
            &enum_checking.local_variables,
            local_variable_create(unicode_view_copy(unicode_view_from_string(instantiated_enum.closures[i].name)),
                                  instantiated_enum.closures[i].what,
                                  optional_value_create(instantiated_enum.closures[i].content), closure_register));
    }
    evaluate_expression_result const evaluated = evaluate_enum_expression(
        &enum_checking, &ignored_instructions,
        enum_expression_create(original.begin, NULL, 0, original.elements, original.element_count));
    instruction_sequence_free(&ignored_instructions);
    local_variable_container_free(enum_checking.local_variables);
    if (enum_checking.register_compile_time_values)
    {
        deallocate(enum_checking.register_compile_time_values);
    }
    if (!evaluated.has_value)
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
    if (evaluated.compile_time_value.value_.type_.kind != type_kind_enumeration)
    {
        LPG_UNREACHABLE();
    }
    size_t const id = root->enum_instantiation_count;
    root->enum_instantiations = reallocate_array(
        root->enum_instantiations, root->enum_instantiation_count + 1, sizeof(*root->enum_instantiations));
    root->enum_instantiations[id] = generic_enum_instantiation_create(
        generic, arguments, argument_count, evaluated.compile_time_value.value_.type_.enum_);
    root->enum_instantiation_count += 1;
    if (argument_types)
    {
        deallocate(argument_types);
    }
    return evaluated;
}

static evaluate_expression_result evaluate_generic_instantiation(function_checking_state *const state,
                                                                 instruction_sequence *const function,
                                                                 generic_instantiation_expression const element)
{
    evaluate_expression_result const generic_evaluated = evaluate_expression(state, function, *element.generic);
    if (!generic_evaluated.has_value)
    {
        return evaluate_expression_result_empty;
    }
    value *const arguments = allocate_array(element.count, sizeof(*arguments));
    type *const argument_types = allocate_array(element.count, sizeof(*argument_types));
    for (size_t i = 0; i < element.count; ++i)
    {
        evaluate_expression_result const argument_evaluated =
            evaluate_expression(state, function, element.arguments[i]);
        if (!argument_evaluated.has_value)
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
            state->on_error(semantic_error_create(semantic_error_expected_compile_time_value,
                                                  expression_source_begin(element.arguments[i])),
                            state->user);
            return evaluate_expression_result_empty;
        }
        arguments[i] = argument_evaluated.compile_time_value.value_;
        argument_types[i] = argument_evaluated.type_;
    }
    if (!generic_evaluated.compile_time_value.is_set ||
        (generic_evaluated.compile_time_value.value_.kind != value_kind_generic_enum))
    {
        if (arguments)
        {
            deallocate(arguments);
        }
        if (argument_types)
        {
            deallocate(argument_types);
        }
        state->on_error(
            semantic_error_create(semantic_error_expected_generic_type, expression_source_begin(*element.generic)),
            state->user);
        return evaluate_expression_result_empty;
    }
    return instantiate_generic_enum(state, function, generic_evaluated.compile_time_value.value_.generic_enum,
                                    arguments, element.count, argument_types,
                                    expression_source_begin(*element.generic));
}

static evaluate_expression_result return_module(function_checking_state *const state,
                                                instruction_sequence *const function, module const current_module)
{
    register_id const where = allocate_register(&state->used_registers);
    write_register_compile_time_value(state, where, current_module.content);
    add_instruction(function, instruction_create_literal(
                                  literal_instruction_create(where, current_module.content, current_module.schema)));
    return evaluate_expression_result_create(
        true, where, current_module.schema, optional_value_create(current_module.content), true, false);
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
            return return_module(state, function, current_module);
        }
    }
    load_module_result const imported = load_module(state, unicode_view_from_string(element.name.value));
    if (!imported.loaded.is_set)
    {
        state->on_error(semantic_error_create(semantic_error_import_failed, element.begin), state->user);
        return evaluate_expression_result_empty;
    }
    module const loaded = module_create(
        unicode_view_copy(unicode_view_from_string(element.name.value)), imported.loaded.value_, imported.schema);
    add_module(state->root, loaded);
    return return_module(state, function, loaded);
}

static evaluate_expression_result evaluate_expression(function_checking_state *const state,
                                                      instruction_sequence *const function, expression const element)
{
    switch (element.type)
    {
    case expression_type_import:
        return evaluate_import(state, function, element.import);

    case expression_type_generic_instantiation:
        return evaluate_generic_instantiation(state, function, element.generic_instantiation);

    case expression_type_type_of:
        return evaluate_type_of(state, function, element.type_of);

    case expression_type_instantiate_struct:
        return evaluate_instantiate_struct(state, function, element.instantiate_struct);

    case expression_type_interface:
        return evaluate_interface(state, function, element.interface);

    case expression_type_impl:
        return evaluate_impl(state, function, element.impl);

    case expression_type_lambda:
        return evaluate_lambda(state, function, element.lambda);

    case expression_type_call:
        return evaluate_call_expression(state, function, element.call);

    case expression_type_binary:
        LPG_TO_DO();

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
            true, where, what, optional_value_create(value_from_integer(element.integer_literal.value)), true, false);
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
            true, result, element_read.type_, element_read.compile_time_value, element_read.is_pure, false);
    }

    case expression_type_comment:
        return make_compile_time_unit();

    case expression_type_not:
        return evaluate_not_expression(state, function, &element);

    case expression_type_match:
        return evaluate_match_expression(state, function, &element);

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
                                      result, value_from_string_ref(literal), type_from_string_ref())));
        write_register_compile_time_value(state, result, value_from_string_ref(literal));
        type const string_type = {type_kind_string_ref, {0}};
        return evaluate_expression_result_create(
            true, result, string_type, optional_value_create(value_from_string_ref(literal)), true, false);
    }

    case expression_type_identifier:
    {
        unicode_view const name = unicode_view_from_string(element.identifier.value);
        evaluate_expression_result const address = read_variable(state, function, name, element.identifier.source);
        return address;
    }

    case expression_type_return:
        return evaluate_return_expression(state, function, &element);

    case expression_type_loop:
    {
        instruction_sequence body = {NULL, 0};
        bool const previous_is_in_loop = state->is_in_loop;
        state->is_in_loop = true;
        evaluate_expression_result const loop_body_result = check_sequence(state, &body, element.loop_body);
        ASSUME(state->is_in_loop);
        state->is_in_loop = previous_is_in_loop;
        register_id const unit_goes_into = allocate_register(&state->used_registers);
        add_instruction(function, instruction_create_loop(loop_instruction_create(unit_goes_into, body)));
        evaluate_expression_result const loop_result = {true,
                                                        unit_goes_into,
                                                        type_from_unit(),
                                                        optional_value_create(value_from_unit()),
                                                        true,
                                                        loop_body_result.always_returns};
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
            state->on_error(
                semantic_error_create(semantic_error_break_outside_of_loop, expression_source_begin(element)),
                state->user);
        }
        return evaluate_expression_result_create(true, into, type_from_unit(), optional_value_empty, false, false);
    }

    case expression_type_sequence:
        return check_sequence(state, function, element.sequence);

    case expression_type_declare:
    {
        instruction_checkpoint const before_initialization = make_checkpoint(state, function);
        evaluate_expression_result const initializer =
            evaluate_expression(state, function, *element.declare.initializer);
        if (!initializer.has_value)
        {
            return evaluate_expression_result_empty;
        }

        if (initializer.compile_time_value.is_set)
        {
            restore_instructions(before_initialization);
            add_instruction(
                function, instruction_create_literal(literal_instruction_create(
                              initializer.where, initializer.compile_time_value.value_, initializer.type_)));
        }

        register_id final_where = initializer.where;
        type final_type = initializer.type_;
        optional_value final_compile_time_value = initializer.compile_time_value;

        if (element.declare.optional_type)
        {
            instruction_checkpoint const previous_code = make_checkpoint(state, function);
            evaluate_expression_result const declared_type =
                evaluate_expression(state, function, *element.declare.optional_type);
            restore(previous_code);
            if (declared_type.has_value)
            {
                if (!declared_type.compile_time_value.is_set || declared_type.type_.kind != type_kind_type)
                {
                    state->on_error(semantic_error_create(semantic_error_expected_compile_time_type,
                                                          expression_source_begin(*element.declare.optional_type)),
                                    state->user);
                }
                else
                {
                    conversion_result const converted =
                        convert(state, function, initializer.where, initializer.type_, initializer.compile_time_value,
                                expression_source_begin(*element.declare.initializer),
                                declared_type.compile_time_value.value_.type_, false);
                    if (converted.ok == failure)
                    {
                        return make_compile_time_unit();
                    }
                    final_where = converted.where;
                    final_type = declared_type.compile_time_value.value_.type_;
                    final_compile_time_value = converted.compile_time_value;
                }
            }
        }

        if (local_variable_name_exists(state->local_variables, unicode_view_from_string(element.declare.name.value)))
        {
            state->on_error(
                semantic_error_create(semantic_error_declaration_with_existing_name, element.declare.name.source),
                state->user);
        }
        else
        {
            add_local_variable(
                &state->local_variables,
                local_variable_create(unicode_view_copy(unicode_view_from_string(element.declare.name.value)),
                                      final_type, final_compile_time_value, final_where));
            define_register_debug_name(
                state, final_where, unicode_view_copy(unicode_view_from_string(element.declare.name.value)));
        }
        register_id const unit_goes_into = allocate_register(&state->used_registers);
        add_instruction(function, instruction_create_literal(
                                      literal_instruction_create(unit_goes_into, value_from_unit(), type_from_unit())));
        write_register_compile_time_value(state, unit_goes_into, value_from_unit());
        evaluate_expression_result const result = {
            true, unit_goes_into, type_from_unit(), optional_value_create(value_from_unit()), false, false};
        return result;
    }

    case expression_type_tuple:
        return evaluate_tuple_expression(state, function, element.tuple);

    case expression_type_struct:
        return evaluate_struct(state, function, element.struct_);

    case expression_type_placeholder:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result
evaluate_return_expression(function_checking_state *state, instruction_sequence *sequence, const expression *expression)
{
    evaluate_expression_result const result = evaluate_expression(state, sequence, *expression->return_);
    if (!result.has_value)
    {
        return result;
    }
    if (state->return_type.is_set)
    {
        conversion_result const converted =
            convert(state, sequence, result.where, result.type_, result.compile_time_value,
                    expression_source_begin(*expression), state->return_type.value, true);
        switch (converted.ok)
        {
        case failure:
            return evaluate_expression_result_empty;

        case success:
        {
            state->return_type = optional_type_create_set(converted.result_type);
            register_id const unit_goes_into = allocate_register(&state->used_registers);
            instruction const return_instruction =
                instruction_create_return(return_instruction_create(converted.where, unit_goes_into));
            add_instruction(sequence, return_instruction);
            return evaluate_expression_result_create(
                true, unit_goes_into, type_from_unit(), optional_value_empty, false, true);
        }
        }
        LPG_UNREACHABLE();
    }
    else
    {
        state->return_type = optional_type_create_set(result.type_);
    }
    register_id const unit_goes_into = allocate_register(&state->used_registers);
    instruction const return_instruction =
        instruction_create_return(return_instruction_create(result.where, unit_goes_into));
    add_instruction(sequence, return_instruction);
    return evaluate_expression_result_create(true, unit_goes_into, type_from_unit(), optional_value_empty, false, true);
}

static evaluate_expression_result check_sequence(function_checking_state *const state,
                                                 instruction_sequence *const output, sequence const input)
{
    size_t const previous_number_of_variables = state->local_variables.count;
    return check_sequence_finish(state, output, input, previous_number_of_variables);
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
                      void *user)
{
    structure *const structures = allocate_array(1, sizeof(*structures));
    structures[0] = clone_structure(global);
    checked_program program = {NULL,       0,
                               structures, 1,
                               {NULL},     allocate_array(1, sizeof(*program.functions)),
                               1,          allocate_array(2, sizeof(*program.enums)),
                               2};
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(unicode_string_from_c_str("false"), type_from_unit());
        elements[1] = enumeration_element_create(unicode_string_from_c_str("true"), type_from_unit());
        program.enums[0] = enumeration_create(elements, 2);
    }
    {
        enumeration_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = enumeration_element_create(unicode_string_from_c_str("none"), type_from_unit());
        elements[1] = enumeration_element_create(
            unicode_string_from_c_str("some"),
            type_from_integer_range(integer_range_create(integer_create(0, 0), integer_max())));
        program.enums[1] = enumeration_create(elements, 2);
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
    program_check check_root = {NULL, 0, NULL, 0, NULL, 0, loader, global, globals};
    check_function_result const checked =
        check_function(&check_root, NULL, expression_from_sequence(root), global, on_error, user, &program, NULL, NULL,
                       0, optional_type_create_empty(), true, optional_type_create_empty());
    deallocate(globals);
    if (checked.success)
    {
        ASSUME(checked.capture_count == 0);
        program.functions[0] = checked.function;
    }
    else
    {
        function_pointer *const dummy_signature = allocate(sizeof(*dummy_signature));
        *dummy_signature = function_pointer_create(
            type_from_unit(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());
        program.functions[0] = checked_function_create(dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    program_check_free(check_root);
    return program;
}
