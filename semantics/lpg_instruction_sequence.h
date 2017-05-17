#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct instruction instruction;

typedef struct instruction_sequence
{
    instruction *elements;
    size_t length;
} instruction_sequence;

instruction_sequence instruction_sequence_create(instruction *elements,
                                                 size_t length);
void instruction_sequence_free(instruction_sequence const *value);
bool instruction_sequence_equals(instruction_sequence const *left,
                                 instruction_sequence const *right);
