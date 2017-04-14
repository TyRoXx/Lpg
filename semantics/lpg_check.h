#pragma once
#include "lpg_expression.h"

typedef enum instruction_type
{
    instruction_call,
    instruction_jump
} instruction_type;

typedef uint32_t jump_address;

typedef struct instruction
{
    instruction_type type;
    jump_address jump_destination;
} instruction;

instruction instruction_create_call(void);
instruction instruction_create_jump(jump_address destination);
int instruction_equals(instruction const left, instruction const right);

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
