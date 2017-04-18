#pragma once
#include "lpg_expression.h"

typedef struct instruction instruction;

typedef struct instruction_sequence
{
    instruction *elements;
    size_t length;
} instruction_sequence;

instruction_sequence instruction_sequence_create(instruction *elements,
                                                 size_t length);
void instruction_sequence_free(instruction_sequence const *value);
int instruction_sequence_equals(instruction_sequence const *left,
                                instruction_sequence const *right);

typedef enum instruction_type
{
    instruction_call,
    instruction_loop
} instruction_type;

struct instruction
{
    instruction_type type;
    instruction_sequence loop;
};

instruction instruction_create_call(void);
instruction instruction_create_loop(instruction_sequence body);
void instruction_free(instruction const *value);
int instruction_equals(instruction const left, instruction const right);

typedef struct checked_function
{
    instruction_sequence body;
} checked_function;

void checked_function_free(checked_function const *function);

typedef struct checked_program
{
    checked_function *functions;
    size_t function_count;
} checked_program;

void checked_program_free(checked_program const *program);
checked_program check(sequence const root);
