#include "lpg_interprete.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

static value call_function(checked_function const callee, value const *globals);

typedef enum run_sequence_result
{
    run_sequence_result_break,
    run_sequence_result_continue
} run_sequence_result;

static run_sequence_result run_sequence(instruction_sequence const sequence,
                                        value const *globals, value *registers)
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
            value result;
            if (callee.function_pointer.code)
            {
                result = call_function(*callee.function_pointer.code, globals);
            }
            else
            {
                result = callee.function_pointer.external(
                    arguments, callee.function_pointer.external_environment);
            }
            deallocate(arguments);
            registers[element.call.result] = result;
            break;
        }

        case instruction_loop:
        {
            bool is_running = true;
            while (is_running)
            {
                switch (run_sequence(element.loop, globals, registers))
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

        case instruction_unit:
            registers[element.unit] = value_from_unit();
            break;

        case instruction_string_literal:
            registers[element.string_literal.into] = value_from_string_ref(
                unicode_view_from_string(element.string_literal.value));
            break;

        case instruction_break:
            return run_sequence_result_break;

        case instruction_instantiate_enum:
            registers[element.instantiate_enum.into] =
                value_from_enum_element(element.instantiate_enum.element);
            break;
        }
    }
    return run_sequence_result_continue;
}

static value call_function(checked_function const callee, value const *globals)
{
    value *const registers =
        allocate_array(callee.number_of_registers, sizeof(*registers));
    ASSERT(run_sequence_result_continue ==
           run_sequence(callee.body, globals, registers));
    deallocate(registers);
    return value_from_unit();
}

void interprete(checked_program const program, value const *globals)
{
    checked_function const entry_point = program.functions[0];
    call_function(entry_point, globals);
}
