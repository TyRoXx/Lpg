#pragma once
#include "lpg_instruction_sequence.h"
#include "lpg_register_id.h"

typedef struct checked_function
{
    instruction_sequence body;
    register_id number_of_registers;
} checked_function;

void checked_function_free(checked_function const *function);
