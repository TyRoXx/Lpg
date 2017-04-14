#pragma once
#include "lpg_expression.h"

typedef enum instruction_type
{
    instruction_call
} instruction_type;

typedef struct instruction
{
    instruction_type type;
} instruction;

instruction instruction_create(instruction_type type);

typedef struct checked_function
{
    instruction *body;
    size_t size;
} checked_function;

void checked_function_free(checked_function const *function);

typedef struct checked_program
{
    checked_function *functions;
    size_t function_count;
} checked_program;

void checked_program_free(checked_program const *program);
checked_program check(sequence const root);
