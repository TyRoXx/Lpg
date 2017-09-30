#include "lpg_checked_function.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

checked_function checked_function_create(register_id return_value,
                                         function_pointer *signature,
                                         instruction_sequence body,
                                         register_id number_of_registers)
{
    checked_function const result = {
        return_value, signature, body, number_of_registers};
    return result;
}

void checked_function_free(checked_function const *function)
{
    instruction_sequence_free(&function->body);
    function_pointer_free(function->signature);
    deallocate(function->signature);
}
