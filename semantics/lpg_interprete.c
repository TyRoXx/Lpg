#include "lpg_interprete.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

static value
call_interpreted_function(checked_function const callee, value const *globals,
                          garbage_collector *const gc,
                          checked_function const *const all_functions);

typedef enum run_sequence_result
{
    run_sequence_result_break,
    run_sequence_result_continue
} run_sequence_result;

value call_function(value const callee, value const *const inferred,
                    value *const arguments, value const *globals,
                    garbage_collector *const gc,
                    checked_function const *const all_functions)
{
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    if (callee.function_pointer.code)
    {
        return call_interpreted_function(
            all_functions[callee.function_pointer.code], globals, gc,
            all_functions);
    }
    return callee.function_pointer.external(
        inferred, arguments, gc, callee.function_pointer.external_environment);
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
            value *const arguments =
                allocate_array(element.call.argument_count, sizeof(*arguments));
            LPG_FOR(size_t, j, element.call.argument_count)
            {
                arguments[j] = registers[element.call.arguments[j]];
            }
            value const result = call_function(
                callee, NULL, arguments, globals, gc, all_functions);
            deallocate(arguments);
            registers[element.call.result] = result;
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

static value
call_interpreted_function(checked_function const callee, value const *globals,
                          garbage_collector *const gc,
                          checked_function const *const all_functions)
{
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    value *const registers =
        allocate_array(callee.number_of_registers, sizeof(*registers));
    ASSERT(run_sequence_result_continue ==
           run_sequence(callee.body, globals, registers, gc, all_functions));
    value const return_value = registers[callee.return_value];
    deallocate(registers);
    return return_value;
}

void interprete(checked_program const program, value const *globals,
                garbage_collector *const gc)
{
    checked_function const entry_point = program.functions[0];
    call_interpreted_function(entry_point, globals, gc, program.functions);
}
