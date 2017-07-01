#pragma once
#include "lpg_instruction_sequence.h"
#include "lpg_register_id.h"

typedef struct function_pointer function_pointer;

typedef struct checked_function
{
    register_id return_value;
    function_pointer *signature;
    instruction_sequence body;
    register_id number_of_registers;
} checked_function;

checked_function checked_function_create(register_id return_value,
                                         function_pointer *signature,
                                         instruction_sequence body,
                                         register_id number_of_registers);
void checked_function_free(checked_function const *function);
