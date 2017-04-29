#pragma once
#include "lpg_expression.h"
#include "lpg_parse_expression.h"

typedef uint32_t struct_member_id;

typedef struct structure_member structure_member;

typedef struct structure
{
    structure_member *members;
    struct_member_id count;
} structure;

structure structure_create(structure_member *members, struct_member_id count);
void structure_free(structure const *value);

typedef struct type type;

typedef struct function_pointer
{
    type *result;
    type *arguments;
    size_t arity;
} function_pointer;

typedef enum type_kind
{
    type_kind_structure,
    type_kind_function_pointer,
    type_kind_unit
} type_kind;

struct type
{
    type_kind kind;
    union
    {
        structure structure_;
        function_pointer function_pointer_;
    };
};

void type_free(type const *value);
void type_deallocate(type *value);
type type_from_function_pointer(function_pointer value);
type type_from_unit(void);
type *type_allocate(type const value);

function_pointer function_pointer_create(type *result, type *arguments,
                                         size_t arity);
void function_pointer_free(function_pointer const *value);

struct structure_member
{
    type what;
    unicode_string name;
};

structure_member structure_member_create(type what, unicode_string name);
void struct_member_free(structure_member const *value);

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
    instruction_loop,
    instruction_global,
    instruction_read_struct,
    instruction_unit
} instruction_type;

typedef uint32_t register_id;

typedef struct optional_register_id
{
    int is_set;
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
int read_struct_instruction_equals(read_struct_instruction const left,
                                   read_struct_instruction const right);

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
    };
};

instruction instruction_create_call(call_instruction argument);
instruction instruction_create_global(register_id into);
instruction instruction_create_read_struct(read_struct_instruction argument);
instruction instruction_create_loop(instruction_sequence body);
instruction instruction_create_unit(register_id into);
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

typedef void check_error_handler(source_location, void *);

void checked_program_free(checked_program const *program);
checked_program check(sequence const root, structure const global,
                      check_error_handler *on_error, void *user);
