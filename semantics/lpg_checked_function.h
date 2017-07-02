#pragma once
#include "lpg_instruction_sequence.h"
#include "lpg_register_id.h"
#include <lpg_non_null.h>

typedef struct function_pointer function_pointer;

typedef struct checked_function
{
    register_id return_value;
    function_pointer *signature;
    instruction_sequence body;
    register_id number_of_registers;
} checked_function;

checked_function checked_function_create(
    register_id return_value, LPG_NON_NULL(function_pointer *signature),
    instruction_sequence body, register_id number_of_registers);
void checked_function_free(LPG_NON_NULL(checked_function const *function));
