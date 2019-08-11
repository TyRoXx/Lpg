#include "lpg_instruction_sequence.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_instruction.h"

instruction_sequence instruction_sequence_create(instruction *elements, size_t length, size_t capacity)
{
    instruction_sequence const result = {elements, length, capacity};
    return result;
}

void instruction_sequence_free(instruction_sequence const *freed)
{
    LPG_FOR(size_t, i, freed->length)
    {
        instruction_free(freed->elements + i);
    }
    if (freed->elements)
    {
        deallocate(freed->elements);
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
