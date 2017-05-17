#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_value.h"
#include "lpg_instruction.h"
#include "lpg_structure_member.h"

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
    type const *type_;
    optional_value compile_time_value;
} evaluate_expression_result;

static evaluate_expression_result
evaluate_expression_result_create(register_id const where,
                                  type const *const type_,
                                  optional_value compile_time_value)
{
    ASSUME(type_);
    evaluate_expression_result result = {
        true, where, type_, compile_time_value};
    return result;
}

static evaluate_expression_result const evaluate_expression_result_empty = {
    false, 0, NULL, {false, {{0, 0}}}};

typedef evaluate_expression_result
read_variable_function(struct function_checking_state *, instruction_sequence *,
                       unicode_view, source_location);

typedef struct function_checking_state
{
    register_id used_registers;
    read_variable_function *read_variable;
    structure const *global;
    check_error_handler *on_error;
    void *user;
} function_checking_state;

static function_checking_state
function_checking_state_create(read_variable_function *read_variable,
                               structure const *global,
                               check_error_handler *on_error, void *user)
{
    function_checking_state result = {0, read_variable, global, on_error, user};
    return result;
}

static register_id allocate_register(function_checking_state *state)
{
    register_id id = state->used_registers;
    ++state->used_registers;
    return id;
}

static evaluate_expression_result check_sequence(function_checking_state *state,
                                                 instruction_sequence *output,
                                                 sequence const input);

static unicode_string decode_string_literal(unicode_view const literal)
{
    ASSUME(literal.length >= 2);
    return unicode_string_from_range(literal.begin + 1, literal.length - 2);
}

static type const *get_return_type(type const callee)
{
    switch (callee.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
        return callee.function_pointer_.result;

    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
        LPG_TO_DO();

    case type_kind_referenced:
        return get_return_type(*callee.referenced);

    case type_kind_type:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static type flatten(type const possibly_referenced)
{
    switch (possibly_referenced.kind)
    {
    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_type:
        return possibly_referenced;

    case type_kind_referenced:
        return *possibly_referenced.referenced;
    }
    UNREACHABLE();
}

static bool is_implicitly_convertible(type const from, type const into)
{
    type const flat_from = flatten(from);
    type const flat_into = flatten(into);
    if (flat_from.kind != flat_into.kind)
    {
        return false;
    }
    switch (flat_from.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
        LPG_TO_DO();

    case type_kind_unit:
        return true;

    case type_kind_string_ref:
        return true;

    case type_kind_enumeration:
        /*TODO check properly*/
        return true;

    case type_kind_referenced:
        UNREACHABLE();

    case type_kind_type:
        return true;
    }
    UNREACHABLE();
}

static bool function_parameter_accepts_type(type const function,
                                            size_t const parameter,
                                            type const argument)
{
    switch (function.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
        return (parameter < function.function_pointer_.arity) &&
               is_implicitly_convertible(
                   argument, function.function_pointer_.arguments[parameter]);

    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
        LPG_TO_DO();

    case type_kind_referenced:
        return function_parameter_accepts_type(
            *function.referenced, parameter, argument);

    case type_kind_type:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

typedef struct read_structure_element_result
{
    type const *type_;
    optional_value compile_time_value;
} read_structure_element_result;

static read_structure_element_result
read_structure_element_result_create(type const *type_,
                                     optional_value compile_time_value)
{
    read_structure_element_result result = {type_, compile_time_value};
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
                &type->members[i].what, type->members[i].compile_time_value);
        }
    }
    state->on_error(semantic_error_create(
                        semantic_error_unknown_element, element_name_location),
                    state->user);
    return read_structure_element_result_create(NULL, optional_value_empty);
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element);

typedef struct instruction_checkpoint
{
    function_checking_state *state;
    instruction_sequence *sequence;
    size_t size_of_sequence;
    register_id used_registers;
} instruction_checkpoint;

static instruction_checkpoint make_checkpoint(function_checking_state *state,
                                              instruction_sequence *sequence)
{
    instruction_checkpoint result = {
        state, sequence, sequence->length, state->used_registers};
    return result;
}

static void restore(instruction_checkpoint const previous_code)
{
    previous_code.state->used_registers = previous_code.used_registers;
    for (size_t i = previous_code.size_of_sequence;
         i < previous_code.sequence->length; ++i)
    {
        instruction_free(previous_code.sequence->elements + i);
    }
    previous_code.sequence->length = previous_code.size_of_sequence;
}

static read_structure_element_result
read_element(function_checking_state *state, instruction_sequence *function,
             expression const object_tree,
             const identifier_expression *const element,
             register_id const result)
{
    instruction_checkpoint const previous_code =
        make_checkpoint(state, function);
    evaluate_expression_result const object =
        evaluate_expression(state, function, object_tree);
    if (!object.has_value)
    {
        LPG_TO_DO();
    }

    type const *const actual_type = (object.type_->kind == type_kind_referenced)
                                        ? object.type_->referenced
                                        : object.type_;
    switch (actual_type->kind)
    {
    case type_kind_structure:
        return read_structure_element(
            state, function, &actual_type->structure_, object.where,
            unicode_view_from_string(element->value), element->source, result);

    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
        LPG_TO_DO();

    case type_kind_enumeration:
        state->on_error(
            semantic_error_create(
                semantic_error_no_members_on_enum_elements, element->source),
            state->user);
        return read_structure_element_result_create(NULL, optional_value_empty);

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
                NULL, optional_value_empty);
        }
        type const *const left_side_type =
            object.compile_time_value.value_.type_;
        switch (left_side_type->kind)
        {
        case type_kind_structure:
        case type_kind_function_pointer:
        case type_kind_unit:
        case type_kind_string_ref:
            LPG_TO_DO();

        case type_kind_enumeration:
        {
            enumeration const *const enum_ = &left_side_type->enum_;
            restore(previous_code);
            LPG_FOR(enum_element_id, i, enum_->size)
            {
                if (unicode_view_equals(
                        unicode_view_from_string(element->value),
                        unicode_view_from_string(enum_->elements[i].name)))
                {
                    add_instruction(
                        function,
                        instruction_create_instantiate_enum(
                            instantiate_enum_instruction_create(result, i)));
                    return read_structure_element_result_create(
                        left_side_type, object.compile_time_value);
                }
            }
            state->on_error(
                semantic_error_create(
                    semantic_error_unknown_element, element->source),
                state->user);
            return read_structure_element_result_create(
                NULL, optional_value_empty);
        }

        case type_kind_referenced:
        case type_kind_type:
            LPG_TO_DO();
        }
        break;
    }

    case type_kind_referenced:
        UNREACHABLE();
    }
    UNREACHABLE();
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
        LPG_TO_DO();

    case expression_type_call:
    {
        instruction_checkpoint const previous_code =
            make_checkpoint(state, function);
        evaluate_expression_result const callee =
            evaluate_expression(state, function, *element.call.callee);
        if (!callee.has_value)
        {
            return evaluate_expression_result_empty;
        }
        register_id *const arguments =
            allocate_array(element.call.arguments.length, sizeof(*arguments));
        LPG_FOR(size_t, i, element.call.arguments.length)
        {
            expression const argument_tree = element.call.arguments.elements[i];
            evaluate_expression_result argument =
                evaluate_expression(state, function, argument_tree);
            if (!argument.has_value)
            {
                restore(previous_code);
                deallocate(arguments);
                return evaluate_expression_result_empty;
            }
            if (!function_parameter_accepts_type(
                    *callee.type_, i, *argument.type_))
            {
                restore(previous_code);
                state->on_error(semantic_error_create(
                                    semantic_error_type_mismatch,
                                    expression_source_begin(argument_tree)),
                                state->user);
                deallocate(arguments);
                return evaluate_expression_result_empty;
            }
            arguments[i] = argument.where;
        }
        register_id const result = allocate_register(state);
        add_instruction(
            function, instruction_create_call(call_instruction_create(
                          callee.where, arguments,
                          element.call.arguments.length, result)));
        ASSUME(callee.type_);
        type const *const return_type = get_return_type(*callee.type_);
        if (!return_type)
        {
            LPG_TO_DO();
        }
        return evaluate_expression_result_create(
            result, return_type,
            /*TODO: compile-time function calls*/ optional_value_empty);
    }

    case expression_type_integer_literal:
        LPG_TO_DO();

    case expression_type_access_structure:
    {
        instruction_checkpoint const previous_code =
            make_checkpoint(state, function);
        register_id const result = allocate_register(state);
        read_structure_element_result const element_read =
            read_element(state, function, *element.access_structure.object,
                         &element.access_structure.member, result);
        if (!element_read.type_)
        {
            restore(previous_code);
            return evaluate_expression_result_empty;
        }
        return evaluate_expression_result_create(
            result, element_read.type_, element_read.compile_time_value);
    }

    case expression_type_match:
        LPG_TO_DO();

    case expression_type_string:
    {
        register_id const result = allocate_register(state);
        unicode_view const value =
            unicode_view_from_string(element.string.value);
        add_instruction(
            function,
            instruction_create_string_literal(string_literal_instruction_create(
                decode_string_literal(value), result)));
        static type const string_type = {type_kind_string_ref, {{NULL, 0}}};
        return evaluate_expression_result_create(
            result, &string_type, optional_value_create(value_from_string_ref(
                                      /*TODO: decode string literal*/ value)));
    }

    case expression_type_identifier:
    {
        unicode_view const name =
            unicode_view_from_string(element.identifier.value);
        evaluate_expression_result address = state->read_variable(
            state, function, name, element.identifier.source);
        return address;
    }

    case expression_type_assign:
    case expression_type_return:
        LPG_TO_DO();

    case expression_type_loop:
    {
        instruction_sequence body = {NULL, 0};
        evaluate_expression_result const result =
            check_sequence(state, &body, element.loop_body);
        add_instruction(function, instruction_create_loop(body));
        return result;
    }

    case expression_type_break:
        add_instruction(function, instruction_create_break());
        return evaluate_expression_result_empty;

    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static evaluate_expression_result check_sequence(function_checking_state *state,
                                                 instruction_sequence *output,
                                                 sequence const input)
{
    if (input.length == 0)
    {
        static type const unit_type = {type_kind_unit, {{NULL, 0}}};
        evaluate_expression_result const final_result =
            evaluate_expression_result_create(
                allocate_register(state), &unit_type,
                optional_value_create(value_from_unit()));
        add_instruction(output, instruction_create_unit(final_result.where));
        return final_result;
    }
    evaluate_expression_result final_result = evaluate_expression_result_empty;
    LPG_FOR(size_t, i, input.length)
    {
        final_result = evaluate_expression(state, output, input.elements[i]);
    }
    return final_result;
}

semantic_error semantic_error_create(semantic_error_type type,
                                     source_location where)
{
    semantic_error result = {type, where};
    return result;
}

bool semantic_error_equals(semantic_error const left,
                           semantic_error const right)
{
    return (left.type == right.type) &&
           source_location_equals(left.where, right.where);
}

static evaluate_expression_result read_variable(function_checking_state *state,
                                                instruction_sequence *to,
                                                unicode_view name,
                                                source_location where)
{
    structure const globals = *state->global;
    instruction_checkpoint const previous_code = make_checkpoint(state, to);
    register_id const global = allocate_register(state);
    add_instruction(to, instruction_create_global(global));
    register_id const result = allocate_register(state);
    read_structure_element_result const element_read = read_structure_element(
        state, to, &globals, global, name, where, result);
    if (!element_read.type_)
    {
        restore(previous_code);
        return evaluate_expression_result_empty;
    }
    return evaluate_expression_result_create(
        result, element_read.type_, element_read.compile_time_value);
}

checked_program check(sequence const root, structure const global,
                      check_error_handler *on_error, void *user)
{
    checked_program program = {
        allocate_array(1, sizeof(struct checked_function)), 1};
    program.functions[0].body = instruction_sequence_create(NULL, 0);
    function_checking_state state =
        function_checking_state_create(read_variable, &global, on_error, user);
    check_sequence(&state, &program.functions[0].body, root);
    program.functions[0].number_of_registers = state.used_registers;
    return program;
}
