#pragma once
#include "lpg_type.h"
#include "lpg_parse_expression.h"

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

typedef enum instruction_type
{
    instruction_call,
    instruction_loop,
    instruction_global,
    instruction_read_struct,
    instruction_unit,
    instruction_string_literal
} instruction_type;

typedef uint32_t register_id;

typedef struct optional_register_id
{
    bool is_set;
    register_id value;
} optional_register_id;

optional_register_id optional_register_id_create(register_id value);

static optional_register_id const optional_register_id_empty = {0, 0};

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
void call_instruction_free(call_instruction const *value);

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

typedef struct string_literal_instruction
{
    unicode_string value;
    register_id into;
} string_literal_instruction;

string_literal_instruction
string_literal_instruction_create(unicode_string value, register_id into);
void string_literal_instruction_free(string_literal_instruction const *value);
bool string_literal_instruction_equals(string_literal_instruction const left,
                                       string_literal_instruction const right);

struct instruction
{
    instruction_type type;
    union
    {
        instruction_sequence loop;
        call_instruction call;
        register_id global_into;
        read_struct_instruction read_struct;
        register_id unit;
        string_literal_instruction string_literal;
    };
};

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(instruction_sequence body);
instruction instruction_create_unit(register_id into);
instruction instruction_create_string_literal(string_literal_instruction value);
void instruction_free(instruction const *value);
bool instruction_equals(instruction const left, instruction const right);

typedef struct checked_function
{
    instruction_sequence body;
    register_id number_of_registers;
} checked_function;

void checked_function_free(checked_function const *function);

typedef struct checked_program
{
    checked_function *functions;
    size_t function_count;
} checked_program;

typedef enum semantic_error_type
{
    semantic_error_unknown_identifier
} semantic_error_type;

typedef struct semantic_error
{
    semantic_error_type type;
    source_location where;
} semantic_error;

semantic_error semantic_error_create(semantic_error_type type,
                                     source_location where);
bool semantic_error_equals(semantic_error const left,
                           semantic_error const right);

typedef void check_error_handler(semantic_error, void *);

void checked_program_free(checked_program const *program);
checked_program check(sequence const root, structure const global,
                      check_error_handler *on_error, void *user);
