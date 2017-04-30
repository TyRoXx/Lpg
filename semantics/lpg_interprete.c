#include "lpg_interprete.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

function_pointer_value
function_pointer_value_from_external(external_function *external,
                                     void *environment)
{
    function_pointer_value result = {NULL, external, environment};
    return result;
}

value value_from_flat_object(value const *flat_object)
{
    value result;
    result.flat_object = flat_object;
    return result;
}

value value_from_function_pointer(function_pointer_value function_pointer)
{
    value result;
    result.function_pointer = function_pointer;
    return result;
}

value value_from_string_ref(unicode_view const string_ref)
{
    value result;
    result.string_ref = string_ref;
    return result;
}

value value_from_unit(void)
{
    value result;
    /*dummy value to avoid compiler warning*/
    result.integer_ = integer_create(0, 0);
    return result;
}

static value call_function(checked_function const callee, value const *globals);

static void run_sequence(instruction_sequence const sequence,
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
            run_sequence(element.loop, globals, registers);
            break;

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
            LPG_TO_DO();

        case instruction_string_literal:
            registers[element.string_literal.into] = value_from_string_ref(
                unicode_view_from_string(element.string_literal.value));
            break;
        }
    }
}

static value call_function(checked_function const callee, value const *globals)
{
    value *const registers =
        allocate_array(callee.number_of_registers, sizeof(*registers));
    run_sequence(callee.body, globals, registers);
    deallocate(registers);
    return value_from_unit();
}

void interprete(checked_program const program, value const *globals)
{
    checked_function const entry_point = program.functions[0];
    call_function(entry_point, globals);
}
