#pragma once
#include "lpg_function_id.h"
#include "lpg_instruction_sequence.h"
#include "lpg_integer.h"
#include "lpg_register.h"
#include "lpg_struct_member_id.h"
#include "lpg_value.h"

typedef struct instruction instruction;

typedef enum instruction_type
{
    instruction_call = 1,
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
    instruction_return,
    instruction_instantiate_struct,
    instruction_new_array,
    instruction_current_function
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
void call_instruction_free(LPG_NON_NULL(call_instruction const *freed));
bool call_instruction_equals(call_instruction const left, call_instruction const right);

typedef struct return_instruction
{
    register_id returned_value;
    register_id unit_goes_into;
} return_instruction;

return_instruction return_instruction_create(register_id returned_value, register_id unit_goes_into);
bool return_instruction_equals(return_instruction const left, return_instruction const right);

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
    enum_constructor_type which;
    register_id state;
    type state_type;
} enum_construct_instruction;

enum_construct_instruction enum_construct_instruction_create(register_id into, enum_constructor_type which,
                                                             register_id state, type state_type);
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

typedef struct instantiate_struct_instruction
{
    register_id into;
    struct_id instantiated;
    register_id *arguments;
    size_t argument_count;
} instantiate_struct_instruction;

instantiate_struct_instruction instantiate_struct_instruction_create(register_id into, struct_id instantiated,
                                                                     register_id *arguments, size_t argument_count);
void instantiate_struct_instruction_free(instantiate_struct_instruction const freed);
bool instantiate_struct_instruction_equals(instantiate_struct_instruction const left,
                                           instantiate_struct_instruction const right);

typedef struct loop_instruction
{
    register_id unit_goes_into;
    instruction_sequence body;
} loop_instruction;

loop_instruction loop_instruction_create(register_id unit_goes_into, instruction_sequence body);
void loop_instruction_free(loop_instruction const freed);
bool loop_instruction_equals(loop_instruction const left, loop_instruction const right);

typedef struct new_array_instruction
{
    interface_id result_type;
    register_id into;
    type element_type;
} new_array_instruction;

new_array_instruction new_array_instruction_create(interface_id result_type, register_id into, type element_type);
void new_array_instruction_free(new_array_instruction const freed);
bool new_array_instruction_equals(new_array_instruction const left, new_array_instruction const right);

typedef struct current_function_instruction
{
    register_id into;
} current_function_instruction;

current_function_instruction current_function_instruction_create(register_id into);
void current_function_instruction_free(current_function_instruction const freed);
bool current_function_instruction_equals(current_function_instruction const left,
                                         current_function_instruction const right);

struct instruction
{
    instruction_type type;
    union
    {
        loop_instruction loop;
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
        register_id break_into;
        instantiate_struct_instruction instantiate_struct;
        new_array_instruction new_array;
        current_function_instruction current_function;
    };
};

typedef enum match_instruction_case_kind
{
    match_instruction_case_kind_value = 1,
    match_instruction_case_kind_stateful_enum
} match_instruction_case_kind;

typedef struct match_instruction_case_stateful_enum
{
    enum_element_id element;
    register_id where;
} match_instruction_case_stateful_enum;

match_instruction_case_stateful_enum match_instruction_case_stateful_enum_create(enum_element_id element,
                                                                                 register_id where);

struct match_instruction_case
{
    match_instruction_case_kind kind;
    union
    {
        register_id key_value;
        match_instruction_case_stateful_enum stateful_enum;
    };
    instruction_sequence action;
    optional_register_id value;
};

match_instruction_case match_instruction_case_create_value(register_id key_value, instruction_sequence action,
                                                           optional_register_id returned);
match_instruction_case match_instruction_case_create_stateful_enum(match_instruction_case_stateful_enum stateful_enum,
                                                                   instruction_sequence action,
                                                                   optional_register_id returned);
void match_instruction_case_free(match_instruction_case freed);
bool match_instruction_case_equals(match_instruction_case const left, match_instruction_case const right);

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_return(return_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(loop_instruction loop);
instruction instruction_create_break(register_id const into);
instruction instruction_create_literal(literal_instruction const content);
instruction instruction_create_tuple(tuple_instruction argument);
instruction instruction_create_enum_construct(enum_construct_instruction argument);
instruction instruction_create_match(match_instruction argument);
instruction instruction_create_get_captures(register_id const into);
instruction instruction_create_lambda_with_captures(lambda_with_captures_instruction const argument);
instruction instruction_create_get_method(get_method_instruction const argument);
instruction instruction_create_erase_type(erase_type_instruction const argument);
instruction instruction_create_instantiate_struct(instantiate_struct_instruction const argument);
instruction instruction_create_new_array(new_array_instruction const argument);
instruction instruction_create_current_function(current_function_instruction const argument);

void instruction_free(LPG_NON_NULL(instruction const *freed));
bool instruction_equals(instruction const left, instruction const right);
void add_instruction(LPG_NON_NULL(instruction_sequence *to), instruction const added);
