#include "lpg_interpret.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_instruction.h"
#include "lpg_optional_function_id.h"
#include "lpg_standard_library.h"
#include <string.h>

static external_function_result call_interpreted_function(checked_function const callee,
                                                          optional_function_id const callee_id,
                                                          value const *const captures, optional_value const self,
                                                          value *const arguments, interpreter *const context);

typedef enum run_sequence_result {
    run_sequence_result_break = 1,
    run_sequence_result_continue,
    run_sequence_result_return,
    run_sequence_result_unavailable_at_this_time,
    run_sequence_result_out_of_memory,
    run_sequence_result_stack_overflow,
    run_sequence_result_instruction_limit_reached
} run_sequence_result;

interpreter interpreter_create(value const *globals, garbage_collector *const gc,
                               checked_function *const *const all_functions, lpg_interface *const *const all_interfaces,
                               size_t max_recursion, size_t *current_recursion, uint64_t max_executed_instructions,
                               uint64_t *executed_instructions)
{
    interpreter const result = {globals,
                                gc,
                                all_functions,
                                all_interfaces,
                                max_recursion,
                                current_recursion,
                                max_executed_instructions,
                                executed_instructions};
    return result;
}

external_function_result call_function(function_pointer_value const callee, optional_value const self,
                                       value *const arguments, interpreter *const context)
{
    ASSUME(context);
    if (callee.external)
    {
        if (*context->current_recursion == context->max_recursion)
        {
            return external_function_result_create_stack_overflow();
        }
        *context->current_recursion += 1;
        external_function_result const return_value =
            callee.external(callee.captures, callee.external_environment, self, arguments, context);
        *context->current_recursion -= 1;
        if (!callee.external_signature.result.is_set)
        {
            LPG_TO_DO();
        }
        switch (return_value.code)
        {
        case external_function_result_out_of_memory:
        case external_function_result_unavailable:
        case external_function_result_stack_overflow:
        case external_function_result_instruction_limit_reached:
            break;

        case external_function_result_success:
            ASSUME(value_conforms_to_type(return_value.if_success, callee.external_signature.result.value));
            break;
        }
        return return_value;
    }
    return call_interpreted_function((*context->all_functions)[callee.code], optional_function_id_create(callee.code),
                                     callee.captures, self, arguments, context);
}

typedef struct invoke_method_parameters
{
    function_id method;
    size_t parameter_count;
} invoke_method_parameters;

static external_function_result invoke_method(value const *const captures, void *environment, optional_value const self,
                                              value *const arguments, interpreter *const context)
{
    (void)environment;
    ASSUME(captures);
    value const from = captures[0];
    invoke_method_parameters const *const parameters = environment;
    switch (from.kind)
    {
    case value_kind_generic_struct:
        LPG_TO_DO();

    case value_kind_generic_lambda:
        LPG_TO_DO();

    case value_kind_type_erased:
    {
        implementation *const impl =
            &implementation_ref_resolve(*context->all_interfaces, from.type_erased.impl)->target;
        ASSUME(impl);
        ASSUME(parameters->method < impl->method_count);
        size_t const method_parameter_count = parameters->parameter_count;
        function_pointer_value const *const function = &impl->methods[parameters->method];
        ASSUME(method_parameter_count < SIZE_MAX);
        ASSUME(!self.is_set);
        external_function_result const result =
            call_function(*function, optional_value_create(*from.type_erased.self), arguments, context);
        return result;
    }

    case value_kind_array:
    {
        switch (parameters->method)
        {
        case 0: // size
            return external_function_result_from_success(value_from_integer(integer_create(0, from.array->count)));

        case 1: // load
        {
            value const index = arguments[0];
            ASSUME(index.kind == value_kind_integer);
            if (integer_less(index.integer_, integer_create(0, from.array->count)))
            {
                value *const state = garbage_collector_allocate(context->gc, sizeof(*state));
                *state = from.array->elements[index.integer_.low];
                return external_function_result_from_success(
                    value_from_enum_element(0, from.array->element_type, state));
            }
            return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
        }

        case 2: // store
        {
            value const index = arguments[0];
            ASSUME(index.kind == value_kind_integer);
            if (!integer_less(index.integer_, integer_create(0, from.array->count)))
            {
                return external_function_result_from_success(value_from_enum_element(0, type_from_unit(), NULL));
            }
            value const new_element = arguments[1];
            from.array->elements[index.integer_.low] = new_element;
            return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
        }

        case 3: // append
        {
            value const new_element = arguments[0];
            ASSUME(value_is_valid(new_element));
            if (from.array->count == from.array->allocated)
            {
                size_t new_capacity = (from.array->count * 2);
                if (new_capacity == 0)
                {
                    new_capacity = 1;
                }
                value *const new_elements =
                    garbage_collector_allocate_array(context->gc, new_capacity, sizeof(*new_elements));
                if (from.array->count > 0)
                {
                    memcpy(new_elements, from.array->elements, sizeof(*new_elements) * from.array->count);
                }
                from.array->elements = new_elements;
                from.array->allocated = new_capacity;
            }
            from.array->elements[from.array->count] = new_element;
            from.array->count += 1;
            for (size_t i = 0; i < from.array->count; ++i)
            {
                ASSUME(value_is_valid(from.array->elements[i]));
            }
            return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
        }

        case 4: // clear
        {
            from.array->count = 0;
            return external_function_result_from_success(value_from_unit());
        }

        case 5: // pop
        {
            ASSUME(from.array->count > 0);
            value const count = arguments[0];
            ASSUME(count.kind == value_kind_integer);
            if (integer_less(integer_create(0, from.array->count), count.integer_))
            {
                return external_function_result_from_success(value_from_enum_element(0, type_from_unit(), NULL));
            }
            from.array->count -= (size_t)count.integer_.low;
            return external_function_result_from_success(value_from_enum_element(1, type_from_unit(), NULL));
        }

        default:
            LPG_UNREACHABLE();
        }
    }

    case value_kind_integer:
    case value_kind_string:
    case value_kind_function_pointer:
    case value_kind_structure:
    case value_kind_type:
    case value_kind_enum_element:
    case value_kind_unit:
    case value_kind_tuple:
    case value_kind_enum_constructor:
    case value_kind_pattern:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static run_sequence_result run_sequence(instruction_sequence const sequence, value *const return_value,
                                        value *const registers, structure_value const captures,
                                        optional_function_id const current_function, interpreter *const context);

static void run_new_array(new_array_instruction const new_array, garbage_collector *const gc, value *const registers)
{
    array_value *const array = garbage_collector_allocate(gc, sizeof(*array));
    *array = array_value_create(NULL, 0, 0, new_array.element_type);
    registers[new_array.into] = value_from_array(array);
}

static void run_get_method(get_method_instruction const get_method, garbage_collector *const gc, value *const registers,
                           lpg_interface const *const all_interfaces)
{
    size_t const capture_count = 1;
    value *const pseudo_captures = garbage_collector_allocate_array(gc, capture_count, sizeof(*pseudo_captures));
    ASSUME(value_is_valid(registers[get_method.from]));
    pseudo_captures[0] = registers[get_method.from];
    lpg_interface const *const interface_ = &all_interfaces[get_method.interface_];
    ASSUME(get_method.method < interface_->method_count);
    method_description const method = interface_->methods[get_method.method];
    invoke_method_parameters *const parameters = garbage_collector_allocate(gc, sizeof(*parameters));
    parameters->method = get_method.method;
    parameters->parameter_count = method.parameters.length;
    type *const pseudo_capture_types =
        garbage_collector_allocate_array(gc, capture_count, sizeof(*pseudo_capture_types));
    pseudo_capture_types[0] = type_from_interface(get_method.interface_);
    registers[get_method.into] = value_from_function_pointer(function_pointer_value_from_external(
        invoke_method, parameters, pseudo_captures,
        function_pointer_create(optional_type_create_set(method.result), method.parameters,
                                tuple_type_create(pseudo_capture_types, capture_count), optional_type_create_empty())));
}

static void run_erase_type(erase_type_instruction const erase_type, garbage_collector *const gc, value *const registers)
{
    value *const self = garbage_collector_allocate(gc, sizeof(*self));
    *self = registers[erase_type.self];
    registers[erase_type.into] = value_from_type_erased(type_erased_value_create(erase_type.impl, self));
}

static run_sequence_result run_call(call_instruction const call, value *const registers, interpreter *const context)
{
    value const callee = registers[call.callee];
    ASSUME(value_is_valid(callee));
    if (callee.kind == value_kind_unit)
    {
        return run_sequence_result_unavailable_at_this_time;
    }
    value *const arguments = allocate_array(call.argument_count, sizeof(*arguments));
    LPG_FOR(size_t, j, call.argument_count)
    {
        arguments[j] = registers[call.arguments[j]];
        ASSUME(value_is_valid(arguments[j]));
    }
    switch (callee.kind)
    {
    case value_kind_function_pointer:
    {
        external_function_result const result =
            call_function(callee.function_pointer, optional_value_empty, arguments, context);
        deallocate(arguments);
        switch (result.code)
        {
        case external_function_result_out_of_memory:
            return run_sequence_result_out_of_memory;

        case external_function_result_success:
            ASSUME(value_is_valid(result.if_success));
            registers[call.result] = result.if_success;
            break;

        case external_function_result_unavailable:
            return run_sequence_result_unavailable_at_this_time;

        case external_function_result_stack_overflow:
            return run_sequence_result_stack_overflow;

        case external_function_result_instruction_limit_reached:
            return run_sequence_result_instruction_limit_reached;
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
        LPG_UNREACHABLE();

    case value_kind_enum_constructor:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
    case value_kind_generic_struct:
    case value_kind_generic_lambda:
    case value_kind_array:
        LPG_TO_DO();
    }
    return run_sequence_result_continue;
}

static run_sequence_result run_loop(loop_instruction const loop, value *const return_value, value *const registers,
                                    structure_value const captures, optional_function_id const current_function,
                                    interpreter *const context)
{
    bool is_running = true;
    registers[loop.unit_goes_into] = value_from_unit();
    while (is_running)
    {
        switch (run_sequence(loop.body, return_value, registers, captures, current_function, context))
        {
        case run_sequence_result_break:
            is_running = false;
            break;

        case run_sequence_result_continue:
            break;

        case run_sequence_result_unavailable_at_this_time:
            return run_sequence_result_unavailable_at_this_time;

        case run_sequence_result_return:
            return run_sequence_result_return;

        case run_sequence_result_out_of_memory:
            return run_sequence_result_out_of_memory;

        case run_sequence_result_stack_overflow:
            return run_sequence_result_stack_overflow;

        case run_sequence_result_instruction_limit_reached:
            return run_sequence_result_instruction_limit_reached;
        }
    }
    return run_sequence_result_continue;
}

static void run_tuple(tuple_instruction const tuple_, garbage_collector *const gc, value *const registers)
{
    value new_tuple;
    new_tuple.kind = value_kind_tuple;
    value *values = garbage_collector_allocate_array(gc, tuple_.element_count, sizeof(*values));
    for (size_t j = 0; j < tuple_.element_count; ++j)
    {
        values[j] = registers[*(tuple_.elements + j)];
        ASSUME(value_is_valid(values[j]));
    }
    new_tuple.tuple_ = value_tuple_create(values, tuple_.element_count);
    registers[tuple_.result] = new_tuple;
}

static void run_instantiate_struct(instantiate_struct_instruction const instantiate_struct, garbage_collector *const gc,
                                   value *const registers)
{
    value *const values = garbage_collector_allocate_array(gc, instantiate_struct.argument_count, sizeof(*values));
    for (size_t j = 0; j < instantiate_struct.argument_count; ++j)
    {
        values[j] = registers[*(instantiate_struct.arguments + j)];
        ASSUME(value_is_valid(values[j]));
    }
    registers[instantiate_struct.into] =
        value_from_structure(structure_value_create(values, instantiate_struct.argument_count));
}

static void run_enum_construct(enum_construct_instruction const enum_construct, garbage_collector *const gc,
                               value *const registers)
{
    value *const state = garbage_collector_allocate(gc, sizeof(*state));
    *state = registers[enum_construct.state];
    ASSUME(value_is_valid(*state));
    registers[enum_construct.into] =
        value_from_enum_element(enum_construct.which.which, enum_construct.state_type, state);
}

static run_sequence_result run_match(match_instruction const match, value *const return_value, value *const registers,
                                     structure_value const captures, optional_function_id const current_function,
                                     interpreter *const context)
{
    value const key = registers[match.key];
    ASSUME(value_is_valid(key));
    match_instruction_case *matching_case = NULL;
    match_instruction_case *default_case = NULL;
    for (size_t j = 0; j < match.count; ++j)
    {
        match_instruction_case *const this_case = match.cases + j;
        bool matches = false;
        switch (this_case->kind)
        {
        case match_instruction_case_kind_stateful_enum:
            ASSUME(key.kind == value_kind_enum_element);
            matches = (key.enum_element.which == this_case->stateful_enum.element);
            if (matches)
            {
                registers[this_case->stateful_enum.where] = value_or_unit(key.enum_element.state);
            }
            break;

        case match_instruction_case_kind_value:
            matches = value_equals(registers[this_case->key_value], key);
            break;

        case match_instruction_case_kind_default:
            default_case = this_case;
            break;
        }
        if (!matches)
        {
            continue;
        }
        matching_case = this_case;
        break;
    }
    if (!matching_case)
    {
        matching_case = default_case;
    }
    ASSUME(matching_case);
    switch (run_sequence(matching_case->action, return_value, registers, captures, current_function, context))
    {
    case run_sequence_result_break:
        return run_sequence_result_break;

    case run_sequence_result_continue:
        break;

    case run_sequence_result_unavailable_at_this_time:
        return run_sequence_result_unavailable_at_this_time;

    case run_sequence_result_return:
        return run_sequence_result_return;

    case run_sequence_result_out_of_memory:
        return run_sequence_result_out_of_memory;

    case run_sequence_result_stack_overflow:
        return run_sequence_result_stack_overflow;

    case run_sequence_result_instruction_limit_reached:
        return run_sequence_result_instruction_limit_reached;
    }
    if (matching_case->value.is_set)
    {
        ASSUME(value_is_valid(registers[matching_case->value.value]));
        ASSUME(value_conforms_to_type(registers[matching_case->value.value], match.result_type));
        registers[match.result] = registers[matching_case->value.value];
    }
    return run_sequence_result_continue;
}

static void run_lambda_with_captures(lambda_with_captures_instruction const lambda_with_captures,
                                     garbage_collector *const gc, value *const registers)
{
    value *const inner_captures =
        garbage_collector_allocate_array(gc, lambda_with_captures.capture_count, sizeof(*inner_captures));
    for (size_t j = 0; j < lambda_with_captures.capture_count; ++j)
    {
        value const capture = registers[lambda_with_captures.captures[j]];
        inner_captures[j] = capture;
        ASSUME(value_is_valid(capture));
    }
    registers[lambda_with_captures.into] = value_from_function_pointer(function_pointer_value_from_internal(
        lambda_with_captures.lambda, inner_captures, lambda_with_captures.capture_count));
}

static void run_current_function(current_function_instruction const current_function_instr,
                                 structure_value const captures, value *const registers, garbage_collector *const gc,
                                 optional_function_id const current_function)
{
    value *const inner_captures = garbage_collector_allocate_array(gc, captures.count, sizeof(*inner_captures));
    for (size_t j = 0; j < captures.count; ++j)
    {
        value const capture = captures.members[j];
        inner_captures[j] = capture;
        ASSUME(value_is_valid(capture));
    }
    ASSUME(current_function.is_set);
    registers[current_function_instr.into] = value_from_function_pointer(
        function_pointer_value_from_internal(current_function.value, inner_captures, captures.count));
}

static run_sequence_result run_instruction(value *const return_value, value *const registers,
                                           structure_value const captures, optional_function_id const current_function,
                                           instruction const element, interpreter *const context)
{
    if (*context->executed_instructions == context->max_executed_instructions)
    {
        return run_sequence_result_instruction_limit_reached;
    }
    *context->executed_instructions += 1;
    switch (element.type)
    {
    case instruction_new_array:
        run_new_array(element.new_array, context->gc, registers);
        break;

    case instruction_get_method:
        run_get_method(element.get_method, context->gc, registers, *context->all_interfaces);
        break;

    case instruction_erase_type:
        run_erase_type(element.erase_type, context->gc, registers);
        break;

    case instruction_call:
        return run_call(element.call, registers, context);

    case instruction_return:
        *return_value = registers[element.return_.returned_value];
        ASSUME(value_is_valid(*return_value));
        registers[element.return_.unit_goes_into] = value_from_unit();
        return run_sequence_result_return;

    case instruction_loop:
        return run_loop(element.loop, return_value, registers, captures, current_function, context);

    case instruction_global:
        registers[element.global_into] =
            value_from_structure(structure_value_create(context->globals, standard_library_element_count));
        break;

    case instruction_read_struct:
    {
        structure_value const from = registers[element.read_struct.from_object].structure;
        ASSUME(element.read_struct.member < from.count);
        value const member = from.members[element.read_struct.member];
        ASSUME(value_is_valid(member));
        registers[element.read_struct.into] = member;
        break;
    }

    case instruction_break:
        registers[element.break_into] = value_from_unit();
        return run_sequence_result_break;

    case instruction_literal:
        ASSUME(value_is_valid(element.literal.value_));
        registers[element.literal.into] = element.literal.value_;
        break;

    case instruction_tuple:
        run_tuple(element.tuple_, context->gc, registers);
        break;

    case instruction_instantiate_struct:
        run_instantiate_struct(element.instantiate_struct, context->gc, registers);
        break;

    case instruction_enum_construct:
        run_enum_construct(element.enum_construct, context->gc, registers);
        break;

    case instruction_match:
        return run_match(element.match, return_value, registers, captures, current_function, context);

    case instruction_get_captures:
        registers[element.captures] = value_from_structure(captures);
        break;

    case instruction_lambda_with_captures:
        run_lambda_with_captures(element.lambda_with_captures, context->gc, registers);
        break;

    case instruction_current_function:
        run_current_function(element.current_function, captures, registers, context->gc, current_function);
        break;
    }
    return run_sequence_result_continue;
}

static run_sequence_result run_sequence(instruction_sequence const sequence, value *const return_value,
                                        value *const registers, structure_value const captures,
                                        optional_function_id const current_function, interpreter *const context)
{
    if (sequence.length == 0)
    {
        if (*context->executed_instructions == context->max_executed_instructions)
        {
            return run_sequence_result_instruction_limit_reached;
        }
        // empty sequences count as one instructions each so that we can detect infinite, empty loops in compile time
        // evaluation
        *context->executed_instructions += 1;
    }
    LPG_FOR(size_t, i, sequence.length)
    {
        instruction const element = sequence.elements[i];
        switch (run_instruction(return_value, registers, captures, current_function, element, context))
        {
        case run_sequence_result_break:
            return run_sequence_result_break;

        case run_sequence_result_continue:
            break;

        case run_sequence_result_return:
            return run_sequence_result_return;

        case run_sequence_result_unavailable_at_this_time:
            return run_sequence_result_unavailable_at_this_time;

        case run_sequence_result_out_of_memory:
            return run_sequence_result_out_of_memory;

        case run_sequence_result_stack_overflow:
            return run_sequence_result_stack_overflow;

        case run_sequence_result_instruction_limit_reached:
            return run_sequence_result_instruction_limit_reached;
        }
    }
    return run_sequence_result_continue;
}

external_function_result call_checked_function(checked_function const callee, optional_function_id const callee_id,
                                               value const *const captures, optional_value const self,
                                               value *const arguments, interpreter *const context)
{
    return call_interpreted_function(callee, callee_id, captures, self, arguments, context);
}

static external_function_result call_interpreted_function(checked_function const callee,
                                                          optional_function_id const callee_id,
                                                          value const *const captures, optional_value const self,
                                                          value *const arguments, interpreter *const context)
{
    /*there has to be at least one register for the return value, even if it is
     * unit*/
    ASSUME(callee.number_of_registers >= 1);
    if (*context->current_recursion == context->max_recursion)
    {
        return external_function_result_create_stack_overflow();
    }
    value *const registers = allocate_array(callee.number_of_registers, sizeof(*registers));
    register_id next_register = 0;
    if (callee.signature->self.is_set)
    {
        ASSUME(self.is_set);
        registers[next_register] = self.value_;
        ++next_register;
    }
    else
    {
        ASSUME(!self.is_set);
    }
    for (size_t i = 0; i < callee.signature->parameters.length; ++i)
    {
        type const parameter = callee.signature->parameters.elements[i];
        ASSUME(value_conforms_to_type(arguments[i], parameter));
        registers[next_register] = arguments[i];
        ++next_register;
    }
    value return_value = value_create_invalid();
    *context->current_recursion += 1;
    run_sequence_result const result =
        run_sequence(callee.body, &return_value, registers,
                     structure_value_create(captures, callee.signature->captures.length), callee_id, context);
    *context->current_recursion -= 1;
    switch (result)
    {
    case run_sequence_result_break:
        LPG_UNREACHABLE();

    case run_sequence_result_continue:
    case run_sequence_result_return:
        deallocate(registers);
        if (!callee.signature->result.is_set)
        {
            LPG_TO_DO();
        }
        ASSUME(value_conforms_to_type(return_value, callee.signature->result.value));
        return external_function_result_from_success(return_value);

    case run_sequence_result_unavailable_at_this_time:
        deallocate(registers);
        return external_function_result_create_unavailable();

    case run_sequence_result_out_of_memory:
        deallocate(registers);
        return external_function_result_create_out_of_memory();

    case run_sequence_result_stack_overflow:
        deallocate(registers);
        return external_function_result_create_stack_overflow();

    case run_sequence_result_instruction_limit_reached:
        deallocate(registers);
        return external_function_result_create_instruction_limit_reached();
    }
    LPG_UNREACHABLE();
}

void interpret(checked_program const program, value const *globals, garbage_collector *const gc)
{
    function_id const entry_point_id = 0;
    size_t current_recursion = 0;
    size_t const max_recursion = 200;
    uint64_t const max_executed_instructions = 10000;
    uint64_t executed_instructions = 0;
    interpreter context = interpreter_create(globals, gc, &program.functions, &program.interfaces, max_recursion,
                                             &current_recursion, max_executed_instructions, &executed_instructions);
    call_interpreted_function(program.functions[entry_point_id], optional_function_id_create(entry_point_id), NULL,
                              optional_value_empty, NULL, &context);
}
