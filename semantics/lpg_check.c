#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_value.h"
#include "lpg_instruction.h"
#include "lpg_structure_member.h"
#include "lpg_string_literal.h"
#include "lpg_interpret.h"
#include <string.h>
#include "lpg_local_variable.h"
#include "lpg_instruction_checkpoint.h"

static void add_instruction(instruction_sequence *to, instruction const added)
{
    to->elements =
        reallocate_array(to->elements, (to->length + 1), sizeof(*to->elements));
    to->elements[to->length] = added;
    ++(to->length);
}

struct function_checking_state;

typedef struct evaluate_expression_result
{
    bool has_value;
    register_id where;
    type type_;
    optional_value compile_time_value;
    bool is_pure;
} evaluate_expression_result;

static evaluate_expression_result evaluate_expression_result_create(
    bool const has_value, register_id const where, type const type_,
    optional_value compile_time_value, bool const is_pure)
{
    evaluate_expression_result const result = {
        has_value, where, type_, compile_time_value, is_pure};
    return result;
}

static evaluate_expression_result const evaluate_expression_result_empty = {
    false,
    0,
    {type_kind_type, {NULL}},
    {false, {value_kind_integer, {NULL}}},
    false};

typedef struct optional_capture_index
{
    bool has_value;
    capture_index value;
} optional_capture_index;

static optional_capture_index const optional_capture_index_empty = {false, 0};

static bool optional_capture_index_equals(optional_capture_index const left,
                                          optional_capture_index const right)
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

typedef struct variable_address
{
    optional_capture_index captured_in_current_lambda;

    /*set if captured_in_current_lambda is empty:*/
    register_id local_address;
} variable_address;

static variable_address variable_address_from_local(register_id const local)
{
    variable_address const result = {optional_capture_index_empty, local};
    return result;
}

static variable_address
variable_address_from_capture(capture_index const captured)
{
    variable_address const result = {{true, captured}, 0};
    return result;
}

static bool variable_address_equals(variable_address const left,
                                    variable_address const right)
{
    return optional_capture_index_equals(left.captured_in_current_lambda,
                                         right.captured_in_current_lambda) &&
           (left.local_address == right.local_address);
}

typedef struct capture
{
    variable_address from;
    type what;
} capture;

static capture capture_create(variable_address const from, type const what)
{
    capture const result = {from, what};
    return result;
}

typedef struct function_checking_state
{
    struct function_checking_state *parent;
    capture *captures;
    capture_index capture_count;
    register_id used_registers;
    unicode_string *register_debug_names;
    register_id register_debug_name_count;
    bool is_in_loop;
    structure const *global;
    check_error_handler *on_error;
    local_variable_container local_variables;
    void *user;
    checked_program *program;
    instruction_sequence *body;
} function_checking_state;

static function_checking_state function_checking_state_create(
    function_checking_state *parent, structure const *global,
    check_error_handler *on_error, void *user, checked_program *const program,
    instruction_sequence *const body)
{
    function_checking_state const result = {
        parent, NULL,     0,         0,    NULL,    0,   false,
        global, on_error, {NULL, 0}, user, program, body};
    return result;
}

static register_id allocate_register(register_id *const used_registers)
{
    register_id const id = *used_registers;
    ++(*used_registers);
    return id;
}

static evaluate_expression_result
check_sequence(function_checking_state *const state,
               instruction_sequence *const output, sequence const input);

static type get_return_type(type const callee,
                            checked_function const *const all_functions)
{
    switch (callee.kind)
    {
    case type_kind_lambda:
        return all_functions[callee.lambda.lambda].signature->result;

    case type_kind_function_pointer:
        return callee.function_pointer_->result;

    case type_kind_tuple:
        return callee;

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
        return type_from_enumeration(callee.enum_constructor->enumeration);
    }
    LPG_UNREACHABLE();
}

typedef struct type_inference
{
    optional_value *values;
    size_t count;
} type_inference;

static void type_inference_free(type_inference const value)
{
    deallocate(value.values);
}

bool is_implicitly_convertible(type const flat_from, type const flat_into)
{
    if (flat_from.kind != flat_into.kind)
    {
        return false;
    }
    switch (flat_from.kind)
    {
    case type_kind_type:
    case type_kind_unit:
    case type_kind_string_ref:
        return true;

    case type_kind_function_pointer:
        return type_equals(flat_from, flat_into);

    case type_kind_enumeration:
        return flat_from.enum_ == flat_into.enum_;

    case type_kind_tuple:
    {
        if (flat_from.tuple_.length != flat_into.tuple_.length)
        {
            return false;
        }
        for (size_t i = 0; i < flat_from.tuple_.length; ++i)
        {
            if (!is_implicitly_convertible(
                    flat_from.tuple_.elements[i], flat_into.tuple_.elements[i]))
            {
                return false;
            }
        }
        return true;
    }

    case type_kind_integer_range:
        return integer_less_or_equals(flat_into.integer_range_.minimum,
                                      flat_from.integer_range_.minimum) &&
               integer_less_or_equals(flat_from.integer_range_.maximum,
                                      flat_into.integer_range_.maximum);

    case type_kind_lambda:
    case type_kind_structure:
    case type_kind_inferred:
    case type_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static bool check_parameter_type(type const from, type const into,
                                 type_inference const inferring)
{
    if (into.kind == type_kind_inferred)
    {
        ASSUME(into.inferred < inferring.count);
        optional_value *const inferred_parameter_type =
            &inferring.values[into.inferred];
        if (inferred_parameter_type->is_set)
        {
            /*TODO: find conflicts*/
            LPG_TO_DO();
        }
        *inferred_parameter_type = optional_value_create(value_from_type(from));
        return true;
    }
    return is_implicitly_convertible(from, into);
}

static bool function_parameter_accepts_type(
    type const function, size_t const parameter, type const argument,
    type_inference const inferring, checked_function const *const all_functions)
{
    switch (function.kind)
    {
    case type_kind_lambda:
        ASSUME(
            parameter <
            all_functions[function.lambda.lambda].signature->parameters.length);
        return check_parameter_type(
            argument, all_functions[function.lambda.lambda]
                          .signature->parameters.elements[parameter],
            inferring);

    case type_kind_function_pointer:
        ASSUME(parameter < function.function_pointer_->parameters.length);
        return check_parameter_type(
            argument,
            function.function_pointer_->parameters.elements[parameter],
            inferring);

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
        ASSUME(parameter == 0);
        return check_parameter_type(
            argument, function.enum_constructor->enumeration
                          ->elements[function.enum_constructor->which]
                          .state,
            inferring);
    }
    LPG_UNREACHABLE();
}

typedef struct read_structure_element_result
{
    bool success;
    type type_;
    optional_value compile_time_value;
} read_structure_element_result;

static read_structure_element_result
read_structure_element_result_create(bool const success, type const type_,
                                     optional_value const compile_time_value)
{
    read_structure_element_result const result = {
        success, type_, compile_time_value};
    return result;
}

static read_structure_element_result read_structure_element(
    function_checking_state *state, instruction_sequence *function,
    structure const *const type, register_id const where,
    unicode_view const element_name,
    source_location const element_name_location, register_id const result)
{
    LPG_FOR(struct_member_id, i, type->count)
    {
        if (unicode_view_equals(
                element_name, unicode_view_from_string(type->members[i].name)))
        {
            add_instruction(function, instruction_create_read_struct(
                                          read_struct_instruction_create(
                                              where, i, result)));
            return read_structure_element_result_create(
                true, type->members[i].what,
                type->members[i].compile_time_value);
        }
    }
    state->on_error(semantic_error_create(
                        semantic_error_unknown_element, element_name_location),
                    state->user);
    return read_structure_element_result_create(
        false, type_from_unit(), optional_value_empty);
}

static read_structure_element_result
read_tuple_element(function_checking_state *state,
                   instruction_sequence *function, tuple_type const type,
                   register_id const where, unicode_view const element_name,
                   source_location const element_name_location,
                   register_id const result)
{
    integer element_index;
    if (!integer_parse(&element_index, element_name))
    {
        state->on_error(semantic_error_create(semantic_error_unknown_element,
                                              element_name_location),
                        state->user);
        return read_structure_element_result_create(
            false, type_from_unit(), optional_value_empty);
    }
    if (!integer_less(element_index, integer_create(0, type.length)))
    {
        state->on_error(semantic_error_create(semantic_error_unknown_element,
                                              element_name_location),
                        state->user);
        return read_structure_element_result_create(
            false, type_from_unit(), optional_value_empty);
    }
    ASSERT(
        integer_less(element_index, integer_create(0, ~(struct_member_id)0)));
    add_instruction(
        function, instruction_create_read_struct(read_struct_instruction_create(
                      where, (struct_member_id)element_index.low, result)));
    return read_structure_element_result_create(
        true, type.elements[element_index.low], optional_value_empty);
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element);

static read_structure_element_result
read_element(function_checking_state *state, instruction_sequence *function,
             expression const object_tree,
             const identifier_expression *const element,
             register_id const result)
{
    instruction_checkpoint const previous_code =
        make_checkpoint(&state->used_registers, function);
    evaluate_expression_result const object =
        evaluate_expression(state, function, object_tree);
    if (!object.has_value)
    {
        return read_structure_element_result_create(
            false, type_from_unit(), optional_value_empty);
    }

    type const *const actual_type = &object.type_;
    switch (actual_type->kind)
    {
    case type_kind_structure:
        return read_structure_element(
            state, function, actual_type->structure_, object.where,
            unicode_view_from_string(element->value), element->source, result);

    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
    case type_kind_integer_range:
    case type_kind_lambda:
    case type_kind_unit:
    case type_kind_function_pointer:
    case type_kind_string_ref:
        state->on_error(semantic_error_create(
                            semantic_error_unknown_element, element->source),
                        state->user);
        return read_structure_element_result_create(
            false, type_from_unit(), optional_value_empty);

    case type_kind_enumeration:
        state->on_error(
            semantic_error_create(
                semantic_error_no_members_on_enum_elements, element->source),
            state->user);
        return read_structure_element_result_create(
            false, type_from_unit(), optional_value_empty);

    case type_kind_tuple:
        return read_tuple_element(
            state, function, actual_type->tuple_, object.where,
            unicode_view_from_string(element->value), element->source, result);

    case type_kind_type:
    {
        if (!object.compile_time_value.is_set)
        {
            restore(previous_code);
            state->on_error(
                semantic_error_create(
                    semantic_error_expected_compile_time_type, element->source),
                state->user);
            return read_structure_element_result_create(
                false, type_from_unit(), optional_value_empty);
        }
        type const left_side_type = object.compile_time_value.value_.type_;
        switch (left_side_type.kind)
        {
        case type_kind_string_ref:
        case type_kind_unit:
        case type_kind_type:
        case type_kind_integer_range:
            state->on_error(
                semantic_error_create(
                    semantic_error_unknown_element, element->source),
                state->user);
            return read_structure_element_result_create(
                false, type_from_unit(), optional_value_empty);

        case type_kind_structure:
        case type_kind_function_pointer:
        case type_kind_tuple:
        case type_kind_lambda:
        case type_kind_inferred:
        case type_kind_enum_constructor:
            LPG_TO_DO();

        case type_kind_enumeration:
        {
            enumeration const *const enum_ = left_side_type.enum_;
            restore(previous_code);
            LPG_FOR(enum_element_id, i, enum_->size)
            {
                if (unicode_view_equals(
                        unicode_view_from_string(element->value),
                        unicode_view_from_string(enum_->elements[i].name)))
                {
                    if (enum_->elements[i].state.kind == type_kind_unit)
                    {
                        value const literal = value_from_enum_element(i, NULL);
                        add_instruction(
                            function, instruction_create_literal(
                                          literal_instruction_create(
                                              result, literal,
                                              type_from_enumeration(enum_))));
                        return read_structure_element_result_create(
                            true, left_side_type,
                            optional_value_create(literal));
                    }
                    value const literal = value_from_enum_constructor();
                    enum_constructor_type *const constructor_type =
                        garbage_collector_allocate(
                            &state->program->memory, sizeof(*constructor_type));
                    add_instruction(
                        function,
                        instruction_create_literal(literal_instruction_create(
                            result, literal,
                            type_from_enum_constructor(constructor_type))));
                    constructor_type->enumeration = enum_;
                    constructor_type->which = i;
                    return read_structure_element_result_create(
                        true, type_from_enum_constructor(constructor_type),
                        optional_value_create(literal));
                }
            }
            state->on_error(
                semantic_error_create(
                    semantic_error_unknown_element, element->source),
                state->user);
            return read_structure_element_result_create(
                false, type_from_unit(), optional_value_empty);
        }
        }
        LPG_UNREACHABLE();
    }
    }
    LPG_UNREACHABLE();
}

static size_t
expected_call_argument_count(const type callee,
                             checked_function const *const all_functions)
{
    switch (callee.kind)
    {
    case type_kind_lambda:
        return all_functions[callee.lambda.lambda].signature->parameters.length;

    case type_kind_function_pointer:
        return callee.function_pointer_->parameters.length;

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
        ASSUME(callee.enum_constructor->which <
               callee.enum_constructor->enumeration->size);
        return callee.enum_constructor->enumeration
                   ->elements[callee.enum_constructor->which]
                   .state.kind != type_kind_unit;
    }
    LPG_UNREACHABLE();
}

static capture_index
require_capture(LPG_NON_NULL(function_checking_state *const state),
                variable_address const captured, type const what)
{
    for (capture_index i = 0; i < state->capture_count; ++i)
    {
        if (variable_address_equals(state->captures[i].from, captured))
        {
            return i;
        }
    }
    capture_index const result = state->capture_count;
    state->captures = reallocate_array(
        state->captures, (state->capture_count + 1), sizeof(*state->captures));
    state->captures[state->capture_count] = capture_create(captured, what);
    ++(state->capture_count);
    return result;
}

typedef struct read_local_variable_result
{
    /*if exists, the other members are set*/
    bool exists;
    variable_address where;
    type what;
    optional_value compile_time_value;
    bool is_pure;
} read_local_variable_result;

static read_local_variable_result
read_local_variable_result_create(variable_address where, type what,
                                  optional_value compile_time_value,
                                  bool is_pure)
{
    read_local_variable_result const result = {
        true, where, what, compile_time_value, is_pure};
    return result;
}

static read_local_variable_result const read_local_variable_result_empty = {
    false,
    {{false, 0}, 0},
    {type_kind_unit, {NULL}},
    {false, {value_kind_unit, {0}}},
    false};

static read_local_variable_result
read_local_variable(LPG_NON_NULL(function_checking_state *const state),
                    unicode_view const name)
{
    LPG_FOR(size_t, i, state->local_variables.count)
    {
        local_variable const *const variable =
            state->local_variables.elements + i;
        if (unicode_view_equals(unicode_view_from_string(variable->name), name))
        {
            return read_local_variable_result_create(
                variable_address_from_local(variable->where), variable->type_,
                variable->compile_time_value, true);
        }
    }
    if (!state->parent)
    {
        return read_local_variable_result_empty;
    }
    read_local_variable_result const outer_variable =
        read_local_variable(state->parent, name);
    if (!outer_variable.exists)
    {
        return outer_variable;
    }
    capture_index const existing_capture =
        require_capture(state, outer_variable.where, outer_variable.what);
    return read_local_variable_result_create(
        variable_address_from_capture(existing_capture), outer_variable.what,
        outer_variable.compile_time_value, outer_variable.is_pure);
}

static evaluate_expression_result
read_variable(LPG_NON_NULL(function_checking_state *const state),
              LPG_NON_NULL(instruction_sequence *const to),
              unicode_view const name, source_location const where)
{
    instruction_checkpoint const previous_code =
        make_checkpoint(&state->used_registers, to);

    read_local_variable_result const local = read_local_variable(state, name);
    if (local.exists)
    {
        if (local.where.captured_in_current_lambda.has_value)
        {
            register_id const captures =
                allocate_register(&state->used_registers);
            add_instruction(to, instruction_create_get_captures(captures));
            register_id const capture_result =
                allocate_register(&state->used_registers);
            add_instruction(
                to,
                instruction_create_read_struct(read_struct_instruction_create(
                    captures, local.where.captured_in_current_lambda.value,
                    capture_result)));
            return evaluate_expression_result_create(
                true, capture_result, local.what, local.compile_time_value,
                local.is_pure);
        }
        return evaluate_expression_result_create(
            true, local.where.local_address, local.what,
            local.compile_time_value, local.is_pure);
    }

    register_id const global = allocate_register(&state->used_registers);
    add_instruction(to, instruction_create_global(global));
    register_id const result = allocate_register(&state->used_registers);
    read_structure_element_result const element_read = read_structure_element(
        state, to, state->global, global, name, where, result);
    if (!element_read.success)
    {
        restore(previous_code);
        return evaluate_expression_result_empty;
    }
    return evaluate_expression_result_create(true, result, element_read.type_,
                                             element_read.compile_time_value,
                                             false);
}

static evaluate_expression_result make_unit(register_id *const used_registers,
                                            instruction_sequence *output)
{
    type const unit_type = {type_kind_unit, {NULL}};
    evaluate_expression_result const final_result =
        evaluate_expression_result_create(
            true, allocate_register(used_registers), unit_type,
            optional_value_create(value_from_unit()), true);
    add_instruction(
        output, instruction_create_literal(literal_instruction_create(
                    final_result.where, value_from_unit(), type_from_unit())));
    return final_result;
}

static size_t find_lower_bound_for_inferred_values(type const root)
{
    switch (root.kind)
    {
    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_lambda:
    case type_kind_tuple:
    case type_kind_enum_constructor:
        LPG_TO_DO();

    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_type:
    case type_kind_integer_range:
        return 0;

    case type_kind_inferred:
        return (root.inferred + 1);
    }
    LPG_UNREACHABLE();
}

static size_t count_inferred_values(function_pointer const signature)
{
    size_t count = 0;
    for (size_t i = 0; i < signature.parameters.length; ++i)
    {
        size_t const new_count = find_lower_bound_for_inferred_values(
            signature.parameters.elements[i]);
        if (new_count > count)
        {
            count = new_count;
        }
    }
    return count;
}

typedef struct check_function_result
{
    bool success;
    checked_function function;
    capture *captures;
    size_t capture_count;
} check_function_result;

static check_function_result
check_function_result_create(checked_function function, capture *captures,
                             size_t capture_count)
{
    check_function_result const result = {
        true, function, captures, capture_count};
    return result;
}

static check_function_result const check_function_result_empty = {
    false, {0, NULL, {NULL, 0}, NULL, 0}, NULL, 0};

static void define_register_debug_name(function_checking_state *const state,
                                       register_id const which,
                                       unicode_string const name)
{
    register_id const previous_size = state->register_debug_name_count;
    if (which >= previous_size)
    {
        register_id const new_size = (which + 1);
        state->register_debug_names =
            reallocate_array(state->register_debug_names, new_size,
                             sizeof(*state->register_debug_names));
        for (size_t i = previous_size; i < new_size; ++i)
        {
            state->register_debug_names[i] = unicode_string_from_c_str("");
        }
        state->register_debug_name_count = new_size;
    }
    unicode_string_free(state->register_debug_names + which);
    state->register_debug_names[which] = name;
}

static check_function_result check_function(
    function_checking_state *parent, expression const body_in,
    structure const global, check_error_handler *on_error, void *user,
    checked_program *const program, type const *const parameter_types,
    unicode_string *const parameter_names, size_t const parameter_count)
{
    instruction_sequence body_out = instruction_sequence_create(NULL, 0);
    function_checking_state state = function_checking_state_create(
        parent, &global, on_error, user, program, &body_out);

    for (size_t i = 0; i < parameter_count; ++i)
    {
        register_id const address = allocate_register(&state.used_registers);
        add_local_variable(
            &state.local_variables,
            local_variable_create(
                unicode_view_copy(unicode_view_from_string(parameter_names[i])),
                parameter_types[i], optional_value_empty, address));
        define_register_debug_name(
            &state, address,
            unicode_view_copy(unicode_view_from_string(parameter_names[i])));
    }

    evaluate_expression_result const body_evaluated =
        evaluate_expression(&state, &body_out, body_in);

    for (size_t i = 0; i < state.local_variables.count; ++i)
    {
        local_variable *const variable = (state.local_variables.elements + i);
        local_variable_free(variable);
    }
    if (state.local_variables.elements)
    {
        deallocate(state.local_variables.elements);
    }

    register_id return_value;
    if (body_evaluated.has_value)
    {
        return_value = body_evaluated.where;
    }
    else if (body_evaluated.compile_time_value.is_set)
    {
        return_value = allocate_register(&state.used_registers);
        add_instruction(
            &body_out,
            instruction_create_literal(literal_instruction_create(
                return_value, body_evaluated.compile_time_value.value_,
                body_evaluated.type_)));
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

    type *const capture_types =
        state.capture_count
            ? allocate_array(state.capture_count, sizeof(*capture_types))
            : NULL;
    for (size_t i = 0; i < state.capture_count; ++i)
    {
        capture_types[i] = state.captures[i].what;
    }

    function_pointer *const signature = allocate(sizeof(*signature));
    *signature = function_pointer_create(
        body_evaluated.type_, tuple_type_create(NULL, 0),
        tuple_type_create(capture_types, state.capture_count));

    if (state.register_debug_name_count < state.used_registers)
    {
        state.register_debug_names =
            reallocate_array(state.register_debug_names, state.used_registers,
                             sizeof(*state.register_debug_names));
        for (register_id i = state.register_debug_name_count;
             i < state.used_registers; ++i)
        {
            state.register_debug_names[i] = unicode_string_from_c_str("");
        }
    }
    checked_function const result = checked_function_create(
        return_value, signature, body_out, state.register_debug_names,
        state.used_registers);
    return check_function_result_create(
        result, state.captures, state.capture_count);
}

static evaluate_expression_result make_compile_time_unit(void)
{
    evaluate_expression_result const result = {
        false, 0, type_from_unit(), optional_value_create(value_from_unit()),
        true};
    return result;
}

static evaluate_expression_result
evaluate_lambda(function_checking_state *const state,
                instruction_sequence *const function, lambda const evaluated)
{
    type *const parameter_types =
        allocate_array(evaluated.parameter_count, sizeof(*parameter_types));
    unicode_string *const parameter_names =
        allocate_array(evaluated.parameter_count, sizeof(*parameter_names));
    for (size_t i = 0; i < evaluated.parameter_count; ++i)
    {
        instruction_checkpoint const before =
            make_checkpoint(&state->used_registers, function);
        parameter const this_parameter = evaluated.parameters[i];
        evaluate_expression_result const parameter_type =
            evaluate_expression(state, function, *this_parameter.type);
        if (!parameter_type.compile_time_value.is_set)
        {
            LPG_TO_DO();
        }
        if (parameter_type.compile_time_value.value_.kind != value_kind_type)
        {
            LPG_TO_DO();
        }
        parameter_types[i] = parameter_type.compile_time_value.value_.type_;
        parameter_names[i] = unicode_view_copy(
            unicode_view_from_string(this_parameter.name.value));
        restore(before);
    }
    function_id const this_lambda_id = state->program->function_count;
    ++(state->program->function_count);
    state->program->functions = reallocate_array(
        state->program->functions, state->program->function_count,
        sizeof(*state->program->functions));
    {
        function_pointer *const dummy_signature =
            allocate(sizeof(*dummy_signature));
        *dummy_signature = function_pointer_create(type_from_unit(),
                                                   tuple_type_create(NULL, 0),
                                                   tuple_type_create(NULL, 0));
        state->program->functions[this_lambda_id] = checked_function_create(
            0, dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    check_function_result const checked = check_function(
        state, *evaluated.result, *state->global, state->on_error, state->user,
        state->program, parameter_types, parameter_names,
        evaluated.parameter_count);
    for (size_t i = 0; i < evaluated.parameter_count; ++i)
    {
        unicode_string_free(parameter_names + i);
    }
    deallocate(parameter_names);
    if (!checked.success)
    {
        deallocate(parameter_types);
        return evaluate_expression_result_empty;
    }

    ASSUME(checked.function.signature->parameters.length == 0);
    checked.function.signature->parameters.elements = parameter_types;
    checked.function.signature->parameters.length = evaluated.parameter_count;

    checked_function_free(&state->program->functions[this_lambda_id]);
    state->program->functions[this_lambda_id] = checked.function;
    register_id const destination = allocate_register(&state->used_registers);
    type const result_type =
        type_from_lambda(lambda_type_create(this_lambda_id));
    if (checked.captures)
    {
        register_id *const captures =
            allocate_array(checked.capture_count, sizeof(*captures));
        for (size_t i = 0; i < checked.capture_count; ++i)
        {
            capture const current_capture = checked.captures[i];
            if (current_capture.from.captured_in_current_lambda.has_value)
            {
                register_id const captures_register =
                    allocate_register(&state->used_registers);
                add_instruction(function, instruction_create_get_captures(
                                              captures_register));
                register_id const transferred_capture =
                    allocate_register(&state->used_registers);
                add_instruction(
                    function, instruction_create_read_struct(
                                  read_struct_instruction_create(
                                      captures_register,
                                      current_capture.from
                                          .captured_in_current_lambda.value,
                                      transferred_capture)));
                captures[i] = transferred_capture;
            }
            else
            {
                captures[i] = current_capture.from.local_address;
            }
        }
        deallocate(checked.captures);
        add_instruction(function, instruction_create_lambda_with_captures(
                                      lambda_with_captures_instruction_create(
                                          destination, this_lambda_id, captures,
                                          checked.capture_count)));
        return evaluate_expression_result_create(
            true, destination, result_type, optional_value_empty, false);
    }
    value const function_pointer = value_from_function_pointer(
        function_pointer_value_from_internal(this_lambda_id, NULL, 0));
    add_instruction(
        function, instruction_create_literal(literal_instruction_create(
                      destination, function_pointer, result_type)));
    return evaluate_expression_result_create(
        true, destination, result_type, optional_value_create(function_pointer),
        false);
}

static evaluate_expression_result
evaluate_call_expression(function_checking_state *state,
                         instruction_sequence *function, call call)
{
    instruction_checkpoint const previous_code =
        make_checkpoint(&state->used_registers, function);
    evaluate_expression_result const callee =
        evaluate_expression(state, function, *call.callee);
    if (!callee.has_value)
    {
        return make_compile_time_unit();
    }
    size_t inferred_value_count =
        /*needs to be initialized to avoid compiler warnings due to the missing default case in the switch statement below*/ 0;
    switch (callee.type_.kind)
    {
    case type_kind_lambda:
        inferred_value_count = count_inferred_values(
            *state->program->functions[callee.type_.lambda.lambda].signature);
        break;

    case type_kind_function_pointer:
        inferred_value_count =
            count_inferred_values(*callee.type_.function_pointer_);
        break;

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
        inferred_value_count = 0;
        break;
    }
    size_t const expected_arguments =
        expected_call_argument_count(callee.type_, state->program->functions);
    register_id *const arguments =
        allocate_array(expected_arguments, sizeof(*arguments));
    value *const compile_time_arguments =
        allocate_array(expected_arguments, sizeof(*compile_time_arguments));
    bool all_compile_time_arguments = true;
    type_inference inferring = {
        allocate_array(inferred_value_count, sizeof(*inferring.values)),
        inferred_value_count};
    for (size_t i = 0; i < inferred_value_count; ++i)
    {
        inferring.values[i].is_set = false;
    }
    LPG_FOR(size_t, i, call.arguments.length)
    {
        expression const argument_tree = call.arguments.elements[i];
        if (i >= expected_arguments)
        {
            state->on_error(
                semantic_error_create(semantic_error_extraneous_argument,
                                      expression_source_begin(argument_tree)),
                state->user);
            break;
        }
        evaluate_expression_result const argument =
            evaluate_expression(state, function, argument_tree);
        if (!argument.has_value)
        {
            restore(previous_code);
            deallocate(compile_time_arguments);
            deallocate(arguments);
            type_inference_free(inferring);
            return evaluate_expression_result_empty;
        }
        if (!function_parameter_accepts_type(callee.type_, i, argument.type_,
                                             inferring,
                                             state->program->functions))
        {
            restore(previous_code);
            state->on_error(
                semantic_error_create(semantic_error_type_mismatch,
                                      expression_source_begin(argument_tree)),
                state->user);
            deallocate(compile_time_arguments);
            deallocate(arguments);
            type_inference_free(inferring);
            return evaluate_expression_result_empty;
        }
        if (argument.compile_time_value.is_set)
        {
            compile_time_arguments[i] = argument.compile_time_value.value_;
        }
        else
        {
            all_compile_time_arguments = false;
        }
        arguments[i] = argument.where;
    }
    if (call.arguments.length < expected_arguments)
    {
        deallocate(compile_time_arguments);
        restore(previous_code);
        deallocate(arguments);
        type_inference_free(inferring);
        state->on_error(semantic_error_create(semantic_error_missing_argument,
                                              call.closing_parenthesis),
                        state->user);
        return evaluate_expression_result_empty;
    }
    type const return_type =
        get_return_type(callee.type_, state->program->functions);
    register_id result = ~(register_id)0;
    optional_value compile_time_result = {false, value_from_unit()};
    if (callee.compile_time_value.is_set && all_compile_time_arguments)
    {
        value *const complete_inferred_values = allocate_array(
            inferred_value_count, sizeof(*complete_inferred_values));
        for (size_t i = 0; i < inferred_value_count; ++i)
        {
            if (!inferring.values[i].is_set)
            {
                LPG_TO_DO();
            }
            complete_inferred_values[i] = inferring.values[i].value_;
        }
        {
            switch (callee.compile_time_value.value_.kind)
            {
            case value_kind_function_pointer:
            {
                size_t const globals_count = state->global->count;
                value *const globals =
                    allocate_array(globals_count, sizeof(*globals));
                for (size_t i = 0; i < globals_count; ++i)
                {
                    optional_value const compile_time_global =
                        state->global->members[i].compile_time_value;
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
                compile_time_result = call_function(
                    callee.compile_time_value.value_.function_pointer,
                    complete_inferred_values, compile_time_arguments, globals,
                    &state->program->memory, state->program->functions);
                deallocate(globals);
                break;
            }

            case value_kind_integer:
            case value_kind_string:
            case value_kind_flat_object:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
                LPG_UNREACHABLE();

            case value_kind_enum_constructor:
            {
                value *const enum_state = garbage_collector_allocate(
                    &state->program->memory, sizeof(*enum_state));
                ASSUME(expected_arguments == 1);
                *enum_state = compile_time_arguments[0];
                compile_time_result =
                    optional_value_create(value_from_enum_element(
                        callee.type_.enum_constructor->which, enum_state));
                break;
            }
            }
        }
        deallocate(complete_inferred_values);
        if (compile_time_result.is_set)
        {
            deallocate(arguments);
            restore(previous_code);
            result = allocate_register(&state->used_registers);
            if (return_type.kind == type_kind_string_ref)
            {
                size_t const length =
                    compile_time_result.value_.string_ref.length;
                char *const copy =
                    garbage_collector_allocate(&state->program->memory, length);
                memcpy(
                    copy, compile_time_result.value_.string_ref.begin, length);
                add_instruction(
                    function,
                    instruction_create_literal(literal_instruction_create(
                        result, value_from_string_ref(
                                    unicode_view_create(copy, length)),
                        type_from_string_ref())));
            }
            else
            {
                add_instruction(
                    function,
                    instruction_create_literal(literal_instruction_create(
                        result, compile_time_result.value_, return_type)));
            }
        }
    }

    if (!compile_time_result.is_set)
    {
        /*TODO: type inference for runtime-evaluated functions
         * ("templates")*/
        ASSERT(inferred_value_count == 0);
        result = allocate_register(&state->used_registers);
        switch (callee.type_.kind)
        {
        case type_kind_lambda:
        case type_kind_function_pointer:
            add_instruction(
                function,
                instruction_create_call(call_instruction_create(
                    callee.where, arguments, expected_arguments, result)));
            break;

        case type_kind_structure:
        case type_kind_unit:
        case type_kind_string_ref:
        case type_kind_enumeration:
        case type_kind_tuple:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_inferred:
            LPG_UNREACHABLE();

        case type_kind_enum_constructor:
            ASSUME(call.arguments.length == 1);
            add_instruction(
                function, instruction_create_enum_construct(
                              enum_construct_instruction_create(
                                  result, callee.type_.enum_constructor->which,
                                  arguments[0])));
            deallocate(arguments);
            break;
        }
    }

    type_inference_free(inferring);
    deallocate(compile_time_arguments);
    return evaluate_expression_result_create(
        true, result, return_type, compile_time_result, false);
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
        return evaluate_lambda(state, function, element.lambda);

    case expression_type_call:
        return evaluate_call_expression(state, function, element.call);

    case expression_type_integer_literal:
    {
        register_id const where = allocate_register(&state->used_registers);
        type const what = type_from_integer_range(integer_range_create(
            element.integer_literal.value, element.integer_literal.value));
        add_instruction(
            function,
            instruction_create_literal(literal_instruction_create(
                where, value_from_integer(element.integer_literal.value),
                what)));
        return evaluate_expression_result_create(
            true, where, what, optional_value_create(value_from_integer(
                                   element.integer_literal.value)),
            false);
    }

    case expression_type_access_structure:
    {
        instruction_checkpoint const previous_code =
            make_checkpoint(&state->used_registers, function);
        register_id const result = allocate_register(&state->used_registers);
        read_structure_element_result const element_read =
            read_element(state, function, *element.access_structure.object,
                         &element.access_structure.member, result);
        if (!element_read.success)
        {
            restore(previous_code);
            return evaluate_expression_result_empty;
        }
        return evaluate_expression_result_create(
            true, result, element_read.type_, element_read.compile_time_value,
            false);
    }

    case expression_type_comment:
        return make_compile_time_unit();

    case expression_type_match:
    {
        instruction_checkpoint const before =
            make_checkpoint(&state->used_registers, function);
        evaluate_expression_result const key =
            evaluate_expression(state, function, *element.match.input);
        if (!key.has_value)
        {
            return key;
        }
        switch (key.type_.kind)
        {
        case type_kind_enumeration:
            if (key.type_.enum_->size != element.match.number_of_cases)
            {
                state->on_error(
                    semantic_error_create(semantic_error_missing_match_case,
                                          expression_source_begin(element)),
                    state->user);
                return evaluate_expression_result_empty;
            }
            break;

        case type_kind_structure:
        case type_kind_function_pointer:
        case type_kind_unit:
        case type_kind_string_ref:
        case type_kind_lambda:
        case type_kind_tuple:
        case type_kind_type:
        case type_kind_integer_range:
        case type_kind_inferred:
        case type_kind_enum_constructor:
            LPG_TO_DO();
        }
        match_instruction_case *const cases =
            allocate_array(element.match.number_of_cases, sizeof(*cases));
        bool *const enum_elements_handled = allocate_array(
            element.match.number_of_cases, sizeof(*enum_elements_handled));
        memset(enum_elements_handled, 0,
               element.match.number_of_cases * sizeof(*enum_elements_handled));
        type result_type = type_from_unit();
        optional_value compile_time_result = optional_value_empty;
        for (size_t i = 0; i < element.match.number_of_cases; ++i)
        {
            match_case const case_tree = element.match.cases[i];
            evaluate_expression_result const key_evaluated =
                evaluate_expression(state, function, *case_tree.key);
            if (!key_evaluated.has_value)
            {
                for (size_t j = 0; j < i; ++j)
                {
                    match_instruction_case_free(cases[j]);
                }
                if (cases)
                {
                    deallocate(cases);
                }
                deallocate(enum_elements_handled);
                return key_evaluated;
            }

            if (!type_equals(key.type_, key_evaluated.type_))
            {
                state->on_error(semantic_error_create(
                                    semantic_error_type_mismatch,
                                    expression_source_begin(*case_tree.key)),
                                state->user);
                for (size_t j = 0; j < i; ++j)
                {
                    match_instruction_case_free(cases[j]);
                }
                if (cases)
                {
                    deallocate(cases);
                }
                deallocate(enum_elements_handled);
                return evaluate_expression_result_empty;
            }

            {
                ASSUME(key_evaluated.compile_time_value.value_.kind ==
                       value_kind_enum_element);
                bool *const case_handled = (enum_elements_handled +
                                            key_evaluated.compile_time_value
                                                .value_.enum_element.which);
                if (*case_handled)
                {
                    state->on_error(
                        semantic_error_create(
                            semantic_error_duplicate_match_case,
                            expression_source_begin(*case_tree.key)),
                        state->user);
                    for (size_t j = 0; j < i; ++j)
                    {
                        match_instruction_case_free(cases[j]);
                    }
                    if (cases)
                    {
                        deallocate(cases);
                    }
                    deallocate(enum_elements_handled);
                    return evaluate_expression_result_empty;
                }
                *case_handled = true;
            }

            /*TODO: support runtime values as keys?*/
            ASSERT(key_evaluated.compile_time_value.is_set);

            instruction_sequence action = instruction_sequence_create(NULL, 0);
            evaluate_expression_result const action_evaluated =
                evaluate_expression(state, &action, *case_tree.action);
            if (!action_evaluated.has_value)
            {
                for (size_t j = 0; j < i; ++j)
                {
                    match_instruction_case_free(cases[j]);
                }
                if (cases)
                {
                    deallocate(cases);
                }
                instruction_sequence_free(&action);
                deallocate(enum_elements_handled);
                return action_evaluated;
            }

            if (i == 0)
            {
                result_type = action_evaluated.type_;
            }
            else if (!type_equals(result_type, action_evaluated.type_))
            {
                /*TODO: support types that are not the same, but still
                 * comparable*/
                for (size_t j = 0; j < i; ++j)
                {
                    match_instruction_case_free(cases[j]);
                }
                if (cases)
                {
                    deallocate(cases);
                }
                instruction_sequence_free(&action);
                state->on_error(semantic_error_create(
                                    semantic_error_type_mismatch,
                                    expression_source_begin(*case_tree.action)),
                                state->user);
                deallocate(enum_elements_handled);
                return evaluate_expression_result_empty;
            }

            if (!compile_time_result.is_set && key.compile_time_value.is_set &&
                key.is_pure &&
                value_equals(key.compile_time_value.value_,
                             key_evaluated.compile_time_value.value_) &&
                action_evaluated.compile_time_value.is_set &&
                action_evaluated.is_pure)
            {
                compile_time_result = optional_value_create(
                    action_evaluated.compile_time_value.value_);
            }

            cases[i] = match_instruction_case_create(
                key_evaluated.where, action, action_evaluated.where);
        }
        for (size_t i = 0; i < element.match.number_of_cases; ++i)
        {
            ASSUME(enum_elements_handled[i]);
        }
        register_id result_register = allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_match(match_instruction_create(
                          key.where, cases, element.match.number_of_cases,
                          result_register, result_type)));
        deallocate(enum_elements_handled);
        if (compile_time_result.is_set)
        {
            restore(before);
            result_register = allocate_register(&state->used_registers);
            add_instruction(
                function,
                instruction_create_literal(literal_instruction_create(
                    result_register, compile_time_result.value_, result_type)));
            return evaluate_expression_result_create(
                true, result_register, result_type, compile_time_result, true);
        }

        return evaluate_expression_result_create(
            true, result_register, result_type, optional_value_empty, false);
    }

    case expression_type_string:
    {
        register_id const result = allocate_register(&state->used_registers);
        memory_writer decoded = {NULL, 0, 0};
        stream_writer const decoded_writer = memory_writer_erase(&decoded);
        decode_string_literal(
            unicode_view_from_string(element.string.value), decoded_writer);
        char *const copy =
            garbage_collector_allocate(&state->program->memory, decoded.used);
        if (decoded.used > 0)
        {
            memcpy(copy, decoded.data, decoded.used);
        }
        unicode_view const literal = unicode_view_create(copy, decoded.used);
        memory_writer_free(&decoded);
        add_instruction(
            function, instruction_create_literal(literal_instruction_create(
                          result, value_from_string_ref(literal),
                          type_from_string_ref())));
        type const string_type = {type_kind_string_ref, {NULL}};
        return evaluate_expression_result_create(
            true, result, string_type,
            optional_value_create(value_from_string_ref(literal)), true);
    }

    case expression_type_identifier:
    {
        unicode_view const name =
            unicode_view_from_string(element.identifier.value);
        evaluate_expression_result const address =
            read_variable(state, function, name, element.identifier.source);
        return address;
    }

    case expression_type_assign:
    case expression_type_return:
        LPG_TO_DO();

    case expression_type_loop:
    {
        instruction_sequence body = {NULL, 0};
        bool const previous_is_in_loop = state->is_in_loop;
        state->is_in_loop = true;
        /*ignoring body result*/
        check_sequence(state, &body, element.loop_body);
        ASSUME(state->is_in_loop);
        state->is_in_loop = previous_is_in_loop;
        add_instruction(function, instruction_create_loop(body));
        evaluate_expression_result const loop_result = make_compile_time_unit();
        return loop_result;
    }

    case expression_type_break:
        if (state->is_in_loop)
        {
            add_instruction(function, instruction_create_break());
        }
        else
        {
            state->on_error(
                semantic_error_create(semantic_error_break_outside_of_loop,
                                      expression_source_begin(element)),
                state->user);
        }
        return evaluate_expression_result_empty;

    case expression_type_sequence:
        return check_sequence(state, function, element.sequence);

    case expression_type_declare:
    {
        instruction_checkpoint const before_initialization =
            make_checkpoint(&state->used_registers, function);
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
                function,
                instruction_create_literal(literal_instruction_create(
                    initializer.where, initializer.compile_time_value.value_,
                    initializer.type_)));
        }

        if (element.declare.optional_type)
        {
            instruction_checkpoint const previous_code =
                make_checkpoint(&state->used_registers, function);
            evaluate_expression_result const declared_type =
                evaluate_expression(
                    state, function, *element.declare.optional_type);
            restore(previous_code);
            if (declared_type.has_value)
            {
                if (!declared_type.compile_time_value.is_set)
                {
                    state->on_error(
                        semantic_error_create(
                            semantic_error_expected_compile_time_type,
                            expression_source_begin(
                                *element.declare.optional_type)),
                        state->user);
                }
                else if (declared_type.type_.kind != type_kind_type)
                {
                    state->on_error(
                        semantic_error_create(
                            semantic_error_expected_compile_time_type,
                            expression_source_begin(
                                *element.declare.optional_type)),
                        state->user);
                }
                else if (!is_implicitly_convertible(
                             initializer.type_,
                             declared_type.compile_time_value.value_.type_))
                {
                    state->on_error(semantic_error_create(
                                        semantic_error_type_mismatch,
                                        expression_source_begin(
                                            *element.declare.initializer)),
                                    state->user);
                    return make_compile_time_unit();
                }
            }
        }

        if (local_variable_name_exists(
                state->local_variables,
                unicode_view_from_string(element.declare.name.value)))
        {
            state->on_error(semantic_error_create(
                                semantic_error_declaration_with_existing_name,
                                element.declare.name.source),
                            state->user);
        }
        else
        {
            add_local_variable(
                &state->local_variables,
                local_variable_create(
                    unicode_view_copy(
                        unicode_view_from_string(element.declare.name.value)),
                    initializer.type_, initializer.compile_time_value,
                    initializer.where));
            define_register_debug_name(
                state, initializer.where,
                unicode_view_copy(
                    unicode_view_from_string(element.declare.name.value)));
        }
        evaluate_expression_result const result = {
            false, 0, type_from_unit(),
            optional_value_create(value_from_unit()), false};
        return result;
    }

    case expression_type_tuple:
    {
        register_id *registers =
            allocate_array(element.tuple.length, sizeof(*registers));
        tuple_type const tuple_type_for_instruction = {
            allocate_array(element.tuple.length,
                           sizeof(*tuple_type_for_instruction.elements)),
            element.tuple.length};
        tuple_type const tuple_type_for_result = {
            garbage_collector_allocate_array(
                &state->program->memory, element.tuple.length,
                sizeof(*tuple_type_for_instruction.elements)),
            element.tuple.length};
        for (size_t i = 0; i < element.tuple.length; ++i)
        {
            evaluate_expression_result const result =
                evaluate_expression(state, function, element.tuple.elements[i]);
            if (!result.has_value)
            {
                deallocate(registers);
                deallocate(tuple_type_for_instruction.elements);
                return evaluate_expression_result_empty;
            }
            registers[i] = result.where;
            tuple_type_for_instruction.elements[i] = result.type_;
            tuple_type_for_result.elements[i] = result.type_;
        }
        register_id const result_register =
            allocate_register(&state->used_registers);
        add_instruction(
            function, instruction_create_tuple(tuple_instruction_create(
                          registers, element.tuple.length, result_register,
                          tuple_type_for_instruction)));

        return evaluate_expression_result_create(
            true, result_register, type_from_tuple_type(tuple_type_for_result),
            optional_value_empty, false);
    }
    }
    LPG_UNREACHABLE();
}

static evaluate_expression_result
check_sequence(function_checking_state *const state,
               instruction_sequence *const output, sequence const input)
{
    if (input.length == 0)
    {
        return make_unit(&state->used_registers, output);
    }
    bool is_pure = true;
    evaluate_expression_result final_result = evaluate_expression_result_empty;
    size_t const previous_number_of_variables = state->local_variables.count;
    LPG_FOR(size_t, i, input.length)
    {
        final_result = evaluate_expression(state, output, input.elements[i]);
        if (!final_result.is_pure)
        {
            is_pure = false;
        }
    }
    for (size_t i = previous_number_of_variables,
                c = state->local_variables.count;
         i != c; ++i)
    {
        local_variable_free(state->local_variables.elements + i);
    }
    state->local_variables.count = previous_number_of_variables;
    return evaluate_expression_result_create(
        final_result.has_value, final_result.where, final_result.type_,
        final_result.compile_time_value, is_pure);
}

checked_program check(sequence const root, structure const global,
                      check_error_handler *on_error, void *user)
{
    checked_program program = {
        {NULL}, allocate_array(1, sizeof(*program.functions)), 1};
    check_function_result const checked =
        check_function(NULL, expression_from_sequence(root), global, on_error,
                       user, &program, NULL, NULL, 0);
    if (checked.success)
    {
        ASSUME(checked.capture_count == 0);
        program.functions[0] = checked.function;
    }
    else
    {
        function_pointer *const dummy_signature =
            allocate(sizeof(*dummy_signature));
        *dummy_signature = function_pointer_create(type_from_unit(),
                                                   tuple_type_create(NULL, 0),
                                                   tuple_type_create(NULL, 0));
        program.functions[0] = checked_function_create(
            0, dummy_signature, instruction_sequence_create(NULL, 0), NULL, 0);
    }
    return program;
}
