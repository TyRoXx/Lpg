#pragma once
#include "lpg_register_id.h"
#include "lpg_type.h"
#include "lpg_struct_member_id.h"
#include "lpg_enum_element_id.h"
#include "lpg_instruction_sequence.h"

typedef struct instruction instruction;

typedef enum instruction_type
{
    instruction_call,
    instruction_loop,
    instruction_global,
    instruction_read_struct,
    instruction_unit,
    instruction_string_literal,
    instruction_break,
    instruction_instantiate_enum
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
void call_instruction_free(call_instruction const *value);
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

typedef struct instantiate_enum_instruction
{
    register_id into;
    enum_element_id element;
} instantiate_enum_instruction;

instantiate_enum_instruction
instantiate_enum_instruction_create(register_id into, enum_element_id element);
bool instantiate_enum_instruction_equals(
    instantiate_enum_instruction const left,
    instantiate_enum_instruction const right);

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
        instantiate_enum_instruction instantiate_enum;
    };
};

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(instruction_sequence body);
instruction instruction_create_unit(register_id into);
instruction instruction_create_string_literal(string_literal_instruction value);
instruction instruction_create_break(void);
instruction
instruction_create_instantiate_enum(instantiate_enum_instruction value);
void instruction_free(instruction const *value);
bool instruction_equals(instruction const left, instruction const right);