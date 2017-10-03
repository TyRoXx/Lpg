#include "lpg_interpret.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

static optional_value call_interpreted_function(checked_function const callee, value *const arguments,
                                                value const *globals, value const *const captures,
                                                garbage_collector *const gc,
                                                checked_function const *const all_functions);

typedef enum run_sequence_result
{
    run_sequence_result_break,
    run_sequence_result_continue,
    run_sequence_result_unavailable_at_this_time
} run_sequence_result;

optional_value call_function(function_pointer_value const callee, value const *const inferred, value *const arguments,
                             value const *globals, garbage_collector *const gc,
                             checked_function const *const all_functions)
{
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    if (callee.external)
    {
        return optional_value_create(callee.external(inferred, arguments, gc, callee.external_environment));
    }
    return call_interpreted_function(
        all_functions[callee.code], arguments, globals, callee.captures, gc, all_functions);
}

static run_sequence_result run_sequence(instruction_sequence const sequence, value const *globals, value *registers,
                                        value const *const captures, garbage_collector *const gc,
                                        checked_function const *const all_functions)
{
    LPG_FOR(size_t, i, sequence.length)
    {
        instruction const element = sequence.elements[i];
        switch (element.type)
        {
        case instruction_call:
        {
            value const callee = registers[element.call.callee];
            if (callee.kind == value_kind_unit)
            {
                return run_sequence_result_unavailable_at_this_time;
            }
            value *const arguments = allocate_array(element.call.argument_count, sizeof(*arguments));
            LPG_FOR(size_t, j, element.call.argument_count)
            {
                arguments[j] = registers[element.call.arguments[j]];
            }
            switch (callee.kind)
            {
            case value_kind_function_pointer:
            {
                optional_value const result =
                    call_function(callee.function_pointer, NULL, arguments, globals, gc, all_functions);
                deallocate(arguments);
                if (!result.is_set)
                {
                    return run_sequence_result_unavailable_at_this_time;
                }
                registers[element.call.result] = result.value_;
                break;
            }

            case value_kind_integer:
            case value_kind_string:
            case value_kind_flat_object:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
            case value_kind_enum_constructor:
                LPG_TO_DO();
            }
            break;
        }

        case instruction_loop:
        {
            bool is_running = true;
            while (is_running)
            {
                switch (run_sequence(element.loop, globals, registers, captures, gc, all_functions))
                {
                case run_sequence_result_break:
                    is_running = false;
                    break;

                case run_sequence_result_continue:
                    break;

                case run_sequence_result_unavailable_at_this_time:
                    return run_sequence_result_unavailable_at_this_time;
                }
            }
            break;
        }

        case instruction_global:
            registers[element.global_into] = value_from_flat_object(globals);
            break;

        case instruction_read_struct:
        {
            value const *const from = registers[element.read_struct.from_object].flat_object;
            value const member = from[element.read_struct.member];
            registers[element.read_struct.into] = member;
            break;
        }

        case instruction_break:
            return run_sequence_result_break;

        case instruction_literal:
            registers[element.literal.into] = element.literal.value_;
            break;

        case instruction_tuple:
        {
            value value_tuple;
            value_tuple.kind = value_kind_tuple;
            value *values = garbage_collector_allocate_array(gc, element.tuple_.element_count, sizeof(*values));
            for (size_t j = 0; j < element.tuple_.element_count; ++j)
            {
                values[j] = registers[*(element.tuple_.elements + j)];
            }
            value_tuple.tuple_.elements = values;
            registers[element.tuple_.result] = value_tuple;
            break;
        }

        case instruction_enum_construct:
        {
            value *const state = garbage_collector_allocate(gc, sizeof(*state));
            *state = registers[element.enum_construct.state];
            registers[element.enum_construct.into] = value_from_enum_element(element.enum_construct.which, state);
            break;
        }

        case instruction_match:
        {
            value const key = registers[element.match.key];
            for (size_t j = 0; j < element.match.count; ++j)
            {
                match_instruction_case *const this_case = element.match.cases + j;
                if (value_equals(key, registers[this_case->key]))
                {
                    switch (run_sequence(this_case->action, globals, registers, captures, gc, all_functions))
                    {
                    case run_sequence_result_break:
                        return run_sequence_result_break;

                    case run_sequence_result_continue:
                        break;

                    case run_sequence_result_unavailable_at_this_time:
                        return run_sequence_result_unavailable_at_this_time;
                    }
                    registers[element.match.result] = registers[this_case->value];
                    break;
                }
            }
            break;
        }

        case instruction_get_captures:
            registers[element.captures] = value_from_flat_object(captures);
            break;

        case instruction_lambda_with_captures:
        {
            value *const inner_captures = garbage_collector_allocate_array(
                gc, element.lambda_with_captures.capture_count, sizeof(*inner_captures));
            for (size_t j = 0; j < element.lambda_with_captures.capture_count; ++j)
            {
                inner_captures[j] = registers[element.lambda_with_captures.captures[j]];
            }
            registers[element.lambda_with_captures.into] =
                value_from_function_pointer(function_pointer_value_from_internal(
                    element.lambda_with_captures.lambda, inner_captures, element.lambda_with_captures.capture_count));
            break;
        }
        }
    }
    return run_sequence_result_continue;
}

static optional_value call_interpreted_function(checked_function const callee, value *const arguments,
                                                value const *globals, value const *const captures,
                                                garbage_collector *const gc,
                                                checked_function const *const all_functions)
{
    /*there has to be at least one register for the return value, even if it is
     * unit*/
    ASSUME(callee.number_of_registers >= 1);
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    value *const registers = allocate_array(callee.number_of_registers, sizeof(*registers));
    for (size_t i = 0; i < callee.signature->parameters.length; ++i)
    {
        registers[i] = arguments[i];
    }
    switch (run_sequence(callee.body, globals, registers, captures, gc, all_functions))
    {
    case run_sequence_result_break:
        LPG_UNREACHABLE();

    case run_sequence_result_continue:
        ASSUME(callee.return_value < callee.number_of_registers);
        value const return_value = registers[callee.return_value];
        deallocate(registers);
        return optional_value_create(return_value);

    case run_sequence_result_unavailable_at_this_time:
        deallocate(registers);
        return optional_value_empty;
    }
    LPG_UNREACHABLE();
}

void interpret(checked_program const program, value const *globals, garbage_collector *const gc)
{
    checked_function const entry_point = program.functions[0];
    call_interpreted_function(entry_point, NULL, globals, NULL, gc, program.functions);
}
