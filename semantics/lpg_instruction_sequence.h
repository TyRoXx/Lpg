#pragma once
#include <lpg_non_null.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct instruction instruction;

typedef struct instruction_sequence
{
    instruction *elements;
    size_t length;
} instruction_sequence;

instruction_sequence instruction_sequence_create(instruction *elements, size_t length);
void instruction_sequence_free(LPG_NON_NULL(instruction_sequence const *freed));
bool instruction_sequence_equals(LPG_NON_NULL(instruction_sequence const *left),
                                 LPG_NON_NULL(instruction_sequence const *right));
