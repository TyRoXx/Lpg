#include "lpg_checked_function.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"
#include "lpg_assert.h"

checked_function checked_function_create(register_id return_value, function_pointer *signature,
                                         instruction_sequence body, unicode_string *register_debug_names,
                                         register_id number_of_registers)
{
    ASSUME((number_of_registers > 0) || (register_debug_names == NULL));
    checked_function const result = {return_value, signature, body, register_debug_names, number_of_registers};
    return result;
}

void checked_function_free(checked_function const *function)
{
    for (register_id i = 0; i < function->number_of_registers; ++i)
    {
        unicode_string_free(function->register_debug_names + i);
    }
    if (function->number_of_registers > 0)
    {
        deallocate(function->register_debug_names);
    }
    else
    {
        ASSUME(!function->register_debug_names);
    }
    instruction_sequence_free(&function->body);
    function_pointer_free(function->signature);
    deallocate(function->signature);
}
