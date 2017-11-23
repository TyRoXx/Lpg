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
    instruction_tuple,
    instruction_enum_construct,
    instruction_match,
    instruction_get_captures,
    instruction_lambda_with_captures,
    instruction_get_method,
    instruction_erase_type,
    instruction_return
} instruction_type;

typedef struct tuple_instruction
{
    register_id *elements;
    size_t element_count;
    register_id result;
    tuple_type result_type;
} tuple_instruction;

tuple_instruction tuple_instruction_create(register_id *elements, size_t element_count, register_id result,
                                           tuple_type result_type);

typedef struct call_instruction
{
    register_id callee;
    register_id *arguments;
    size_t argument_count;
    register_id result;
} call_instruction;

call_instruction call_instruction_create(register_id callee, register_id *arguments, size_t argument_count,
                                         register_id result);
void call_instruction_free(LPG_NON_NULL(call_instruction const *value));
bool call_instruction_equals(call_instruction const left, call_instruction const right);

typedef struct return_instruction
{
    register_id return_register;
} return_instruction;

return_instruction return_instruction_create(register_id return_register);

typedef struct read_struct_instruction
{
    register_id from_object;
    struct_member_id member;
    register_id into;
} read_struct_instruction;

read_struct_instruction read_struct_instruction_create(register_id from_object, struct_member_id member,
                                                       register_id into);
bool read_struct_instruction_equals(read_struct_instruction const left, read_struct_instruction const right);

typedef struct literal_instruction
{
    register_id into;
    value value_;
    type type_of;
} literal_instruction;

literal_instruction literal_instruction_create(register_id into, value value_, type type_of);
bool literal_instruction_equals(literal_instruction const left, literal_instruction const right);

typedef struct enum_construct_instruction
{
    register_id into;
    enum_element_id which;
    register_id state;
    type state_type;
} enum_construct_instruction;

enum_construct_instruction enum_construct_instruction_create(register_id into, enum_element_id which, register_id state,
                                                             type state_type);
bool enum_construct_instruction_equals(enum_construct_instruction const left, enum_construct_instruction const right);

typedef struct match_instruction_case match_instruction_case;

typedef struct match_instruction
{
    register_id key;
    match_instruction_case *cases;
    size_t count;
    register_id result;
    type result_type;
} match_instruction;

match_instruction match_instruction_create(register_id key, match_instruction_case *cases, size_t count,
                                           register_id result, type result_type);
void match_instruction_free(match_instruction const *match);
bool match_instruction_equals(match_instruction const left, match_instruction const right);

typedef struct lambda_with_captures_instruction
{
    register_id into;
    function_id lambda;
    register_id *captures;
    size_t capture_count;
} lambda_with_captures_instruction;

lambda_with_captures_instruction lambda_with_captures_instruction_create(register_id into, function_id lambda,
                                                                         register_id *captures, size_t capture_count);
void lambda_with_captures_instruction_free(lambda_with_captures_instruction const freed);
bool lambda_with_captures_instruction_equals(lambda_with_captures_instruction const left,
                                             lambda_with_captures_instruction const right);

typedef struct get_method_instruction
{
    interface_id interface_;
    register_id from;
    function_id method;
    register_id into;
} get_method_instruction;

get_method_instruction get_method_instruction_create(interface_id interface_, register_id from, function_id method,
                                                     register_id into);
bool get_method_instruction_equals(get_method_instruction const left, get_method_instruction const right);

typedef struct erase_type_instruction
{
    register_id self;
    register_id into;
    implementation_ref impl;
} erase_type_instruction;

erase_type_instruction erase_type_instruction_create(register_id self, register_id into, implementation_ref impl);
bool erase_type_instruction_equals(erase_type_instruction const left, erase_type_instruction const right);

struct instruction
{
    instruction_type type;
    union
    {
        instruction_sequence loop;
        call_instruction call;
        return_instruction return_;
        register_id global_into;
        read_struct_instruction read_struct;
        literal_instruction literal;
        tuple_instruction tuple_;
        enum_construct_instruction enum_construct;
        match_instruction match;
        register_id captures;
        lambda_with_captures_instruction lambda_with_captures;
        get_method_instruction get_method;
        erase_type_instruction erase_type;
    };
};

struct match_instruction_case
{
    register_id key;
    instruction_sequence action;
    register_id value;
};

match_instruction_case match_instruction_case_create(register_id key, instruction_sequence action, register_id value);
void match_instruction_case_free(match_instruction_case freed);

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_return(return_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(instruction_sequence body);
instruction instruction_create_break(void);
instruction instruction_create_literal(literal_instruction const value);
instruction instruction_create_tuple(tuple_instruction argument);
instruction instruction_create_enum_construct(enum_construct_instruction argument);
instruction instruction_create_match(match_instruction argument);
instruction instruction_create_get_captures(register_id const into);
instruction instruction_create_lambda_with_captures(lambda_with_captures_instruction const argument);
instruction instruction_create_get_method(get_method_instruction const argument);
instruction instruction_create_erase_type(erase_type_instruction const argument);

void instruction_free(LPG_NON_NULL(instruction const *value));
bool instruction_equals(instruction const left, instruction const right);
