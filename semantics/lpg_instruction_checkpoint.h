#pragma once
#include "lpg_register_id.h"
#include "lpg_instruction_sequence.h"

typedef struct instruction_checkpoint
{
    register_id *currently_used_registers;
    instruction_sequence *sequence;
    size_t size_of_sequence;
    register_id originally_used_registers;
} instruction_checkpoint;

instruction_checkpoint make_checkpoint(register_id *const used_registers,
                                       instruction_sequence *sequence);

void restore_instructions(instruction_checkpoint const previous_code);

void restore(instruction_checkpoint const previous_code);
