#pragma once
#include "lpg_function_checking_state.h"
#include "lpg_instruction_sequence.h"
#include "lpg_register.h"

typedef struct instruction_checkpoint
{
    register_id *currently_used_registers;
    register_id *register_compile_time_value_count;
    instruction_sequence *sequence;
    size_t size_of_sequence;
    register_id originally_used_registers;
} instruction_checkpoint;

instruction_checkpoint make_checkpoint(function_checking_state *const state, instruction_sequence *body);

void restore_instructions(instruction_checkpoint const previous_code);

void restore(instruction_checkpoint const previous_code);
