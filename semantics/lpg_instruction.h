#pragma once
#include "lpg_register_id.h"
#include "lpg_struct_member_id.h"
#include "lpg_instruction_sequence.h"
#include "lpg_integer.h"
#include "lpg_value.h"
#include "lpg_function_id.h"

typedef struct instruction instruction;

typedef enum instruction_type
{
    instruction_call,
    instruction_loop,
    instruction_global,
    instruction_read_struct,
    instruction_break,
    instruction_literal,
    instruction_lambda
} instruction_type;

typedef struct call_instruction
{
    register_id callee;
    register_id *arguments;
    size_t argument_count;
    register_id result;
} call_instruction;

call_instruction call_instruction_create(register_id callee,
                                         register_id *arguments,
                                         size_t argument_count,
                                         register_id result);
void call_instruction_free(LPG_NON_NULL(call_instruction const *value));
bool call_instruction_equals(call_instruction const left,
                             call_instruction const right);

typedef struct read_struct_instruction
{
    register_id from_object;
    struct_member_id member;
    register_id into;
} read_struct_instruction;

read_struct_instruction read_struct_instruction_create(register_id from_object,
                                                       struct_member_id member,
                                                       register_id into);
bool read_struct_instruction_equals(read_struct_instruction const left,
                                    read_struct_instruction const right);

typedef struct literal_instruction
{
    register_id into;
    value value_;
} literal_instruction;

literal_instruction literal_instruction_create(register_id into, value value_);
bool literal_instruction_equals(literal_instruction const left,
                                literal_instruction const right);

typedef struct lambda_instruction
{
    register_id into;
    function_id id;
} lambda_instruction;

lambda_instruction lambda_instruction_create(register_id into, function_id id);
bool lambda_instruction_equals(lambda_instruction const left,
                               lambda_instruction const right);

struct instruction
{
    instruction_type type;
    union
    {
        instruction_sequence loop;
        call_instruction call;
        register_id global_into;
        read_struct_instruction read_struct;
        literal_instruction literal;
        lambda_instruction lambda;
    };
};

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(instruction_sequence body);
instruction instruction_create_break(void);
instruction instruction_create_literal(literal_instruction const value);
instruction instruction_create_lambda(lambda_instruction const value);

void instruction_free(LPG_NON_NULL(instruction const *value));
bool instruction_equals(instruction const left, instruction const right);
