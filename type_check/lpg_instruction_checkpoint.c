#include "lpg_instruction_checkpoint.h"
#include "lpg_instruction.h"

instruction_checkpoint make_checkpoint(function_checking_state *const state, instruction_sequence *body)
{
    instruction_checkpoint const result = {
        &state->used_registers, &state->register_compile_time_value_count, body, body->length, state->used_registers};
    return result;
}

void restore_instructions(instruction_checkpoint const previous_code)
{
    for (size_t i = previous_code.size_of_sequence; i < previous_code.sequence->length; ++i)
    {
        instruction_free(previous_code.sequence->elements + i);
    }
    previous_code.sequence->length = previous_code.size_of_sequence;
}

void restore(instruction_checkpoint const previous_code)
{
    if (*previous_code.register_compile_time_value_count > previous_code.originally_used_registers)
    {
        *previous_code.register_compile_time_value_count = previous_code.originally_used_registers;
    }
    *previous_code.currently_used_registers = previous_code.originally_used_registers;
    restore_instructions(previous_code);
}
