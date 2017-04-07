#pragma once
#include "lpg_unicode_string.h"
#include "lpg_integer.h"

typedef struct expression expression;

void expression_deallocate(expression *this);

typedef struct lambda
{
    expression *parameter_type;
    expression *parameter_name;
    expression *result;
} lambda;

lambda lambda_create(expression *parameter_type, expression *parameter_name,
                     expression *result);
void lambda_free(lambda *this);

typedef struct integer_range
{
    integer minimum;
    integer maximum;
} integer_range;

typedef enum expression_type
{
    expression_type_lambda,
    expression_type_builtin,
    expression_type_call,
    expression_type_local,
    expression_type_integer_literal,
    expression_type_integer_range,
    expression_type_function,
    expression_type_add_member,
    expression_type_fill_structure,
    expression_type_access_structure,
    expression_type_add_to_variant,
    expression_type_match,
    expression_type_string,
    expression_type_identifier,
    expression_type_assign,
    expression_type_return,
    expression_type_loop,
    expression_type_break,
    expression_type_sequence
} expression_type;

typedef enum builtin
{
    builtin_unit,
    builtin_empty_structure,
    builtin_empty_variant
} builtin;

typedef struct call
{
    expression *callee;
    expression *arguments;
    size_t number_of_arguments;
} call;

call call_create(expression *callee, expression *arguments,
                 size_t number_of_arguments);

typedef struct function
{
    expression *result;
    expression *parameter;
} function;

void function_free(function *this);

typedef struct add_member
{
    expression *base;
    expression *name;
    expression *type;
} add_member;

void add_member_free(add_member *this);

typedef struct fill_structure
{
    expression *members;
    size_t number_of_members;
} fill_structure;

typedef struct access_structure
{
    expression *object;
    expression *member;
} access_structure;

access_structure access_structure_create(expression *object,
                                         expression *member);

typedef struct add_to_variant
{
    expression *base;
    expression *new_type;
} add_to_variant;

typedef struct match_case
{
    expression *key;
    expression *action;
} match_case;

typedef struct match
{
    expression *input;
    match_case *cases;
    size_t number_of_cases;
} match;

typedef struct assign
{
    expression *left;
    expression *right;
} assign;

assign assign_create(expression *left, expression *right);

typedef struct sequence
{
    expression *elements;
    size_t length;
} sequence;

sequence sequence_create(expression *elements, size_t length);
expression expression_from_assign(assign value);
expression expression_from_return(expression *value);
expression expression_from_loop(expression *body);
expression expression_from_break(void);
expression expression_from_sequence(sequence value);
expression expression_from_access_structure(access_structure value);

struct expression
{
    expression_type type;
    union
    {
        lambda lambda;
        builtin builtin;
        call call;
        expression *local;
        integer integer_literal;
        integer_range integer_range;
        function function;
        add_member add_member;
        fill_structure fill_structure;
        access_structure access_structure;
        add_to_variant add_to_variant;
        match match;
        unicode_string string;
        unicode_string identifier;
        assign assign;
        expression *return_;
        expression *loop_body;
        sequence sequence;
    };
};

expression expression_from_builtin(builtin value);
expression expression_from_integer_literal(integer value);
void call_free(call *this);
void fill_structure_free(fill_structure *this);
void access_structure_free(access_structure *this);
void add_to_variant_free(add_to_variant *this);
void match_free(match *this);
expression expression_from_lambda(lambda lambda);
expression expression_from_unicode_string(unicode_string value);
expression expression_from_call(call value);
expression expression_from_identifier(unicode_string identifier);
expression *expression_allocate(expression value);
void expression_free(expression *this);
