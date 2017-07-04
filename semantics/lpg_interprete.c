#include "lpg_interprete.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

static optional_value
call_interpreted_function(checked_function const callee, value const *globals,
                          garbage_collector *const gc,
                          checked_function const *const all_functions);

typedef enum run_sequence_result
{
    run_sequence_result_break,
    run_sequence_result_continue,
    run_sequence_result_unavailable_at_this_time
} run_sequence_result;

optional_value call_function(function_pointer_value const callee,
                             value const *const inferred,
                             value *const arguments, value const *globals,
                             garbage_collector *const gc,
                             checked_function const *const all_functions)
{
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    if (callee.external)
    {
        return optional_value_create(callee.external(
            inferred, arguments, gc, callee.external_environment));
    }
    return call_interpreted_function(
        all_functions[callee.code], globals, gc, all_functions);
}

static run_sequence_result
run_sequence(instruction_sequence const sequence, value const *globals,
             value *registers, garbage_collector *const gc,
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
            value *const arguments =
                allocate_array(element.call.argument_count, sizeof(*arguments));
            LPG_FOR(size_t, j, element.call.argument_count)
            {
                arguments[j] = registers[element.call.arguments[j]];
            }
            ASSUME(callee.kind == value_kind_function_pointer);
            optional_value const result =
                call_function(callee.function_pointer, NULL, arguments, globals,
                              gc, all_functions);
            deallocate(arguments);
            if (!result.is_set)
            {
                return run_sequence_result_unavailable_at_this_time;
            }
            registers[element.call.result] = result.value_;
            break;
        }

        case instruction_loop:
        {
            bool is_running = true;
            while (is_running)
            {
                switch (run_sequence(
                    element.loop, globals, registers, gc, all_functions))
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
            value const *const from =
                registers[element.read_struct.from_object].flat_object;
            value const member = from[element.read_struct.member];
            registers[element.read_struct.into] = member;
            break;
        }

        case instruction_break:
            return run_sequence_result_break;

        case instruction_literal:
            registers[element.literal.into] = element.literal.value_;
            break;

        case instruction_lambda:
            registers[element.lambda.into] = value_from_function_pointer(
                function_pointer_value_from_internal(element.lambda.id));
            break;
        }
    }
    return run_sequence_result_continue;
}

static optional_value
call_interpreted_function(checked_function const callee, value const *globals,
                          garbage_collector *const gc,
                          checked_function const *const all_functions)
{
    /*there has to be at least one register for the return value, even if it is
     * unit*/
    ASSUME(callee.number_of_registers >= 1);
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    value *const registers =
        allocate_array(callee.number_of_registers, sizeof(*registers));
    switch (run_sequence(callee.body, globals, registers, gc, all_functions))
    {
    case run_sequence_result_break:
        UNREACHABLE();

    case run_sequence_result_continue:
        ASSUME(callee.return_value < callee.number_of_registers);
        value const return_value = registers[callee.return_value];
        deallocate(registers);
        return optional_value_create(return_value);

    case run_sequence_result_unavailable_at_this_time:
        deallocate(registers);
        return optional_value_empty;
    }
    UNREACHABLE();
}

void interprete(checked_program const program, value const *globals,
                garbage_collector *const gc)
{
    checked_function const entry_point = program.functions[0];
    call_interpreted_function(entry_point, globals, gc, program.functions);
}
