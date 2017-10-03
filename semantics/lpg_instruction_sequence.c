#include "lpg_instruction_sequence.h"
#include "lpg_for.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"

instruction_sequence instruction_sequence_create(instruction *elements, size_t length)
{
    instruction_sequence const result = {elements, length};
    return result;
}

void instruction_sequence_free(instruction_sequence const *value)
{
    LPG_FOR(size_t, i, value->length)
    {
        instruction_free(value->elements + i);
    }
    if (value->elements)
    {
        deallocate(value->elements);
    }
}

bool instruction_sequence_equals(instruction_sequence const *left, instruction_sequence const *right)
{
    if (left->length != right->length)
    {
        return 0;
    }
    LPG_FOR(size_t, i, left->length)
    {
        if (!instruction_equals(left->elements[i], right->elements[i]))
        {
            return 0;
        }
    }
    return 1;
}
