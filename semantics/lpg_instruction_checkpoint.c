#include "lpg_instruction_checkpoint.h"
#include "lpg_instruction.h"

instruction_checkpoint make_checkpoint(register_id *const used_registers,
                                       instruction_sequence *sequence)
{
    instruction_checkpoint result = {
        used_registers, sequence, sequence->length, *used_registers};
    return result;
}

void restore_instructions(instruction_checkpoint const previous_code)
{
    for (size_t i = previous_code.size_of_sequence;
         i < previous_code.sequence->length; ++i)
    {
        instruction_free(previous_code.sequence->elements + i);
    }
    previous_code.sequence->length = previous_code.size_of_sequence;
}

void restore(instruction_checkpoint const previous_code)
{
    *previous_code.currently_used_registers =
        previous_code.originally_used_registers;
    restore_instructions(previous_code);
}
