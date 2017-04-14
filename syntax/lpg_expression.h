#pragma once
#include "lpg_unicode_string.h"
#include "lpg_integer.h"

typedef struct expression expression;

void expression_deallocate(expression *this);

typedef struct parameter
{
    expression *name;
    expression *type;
} parameter;

parameter parameter_create(expression *name, expression *type);
void parameter_free(parameter *value);

typedef struct lambda
{
    parameter *parameters;
    size_t parameter_count;
    expression *result;
} lambda;

lambda lambda_create(parameter *parameters, size_t parameter_count,
                     expression *result);
void lambda_free(lambda const *this);

typedef enum expression_type
{
    expression_type_lambda,
    expression_type_call,
    expression_type_integer_literal,
    expression_type_access_structure,
    expression_type_match,
    expression_type_string,
    expression_type_identifier,
    expression_type_make_identifier,
    expression_type_assign,
    expression_type_return,
    expression_type_loop,
    expression_type_break,
    expression_type_sequence,
    expression_type_declare,
    expression_type_tuple
} expression_type;

typedef struct tuple
{
    expression *elements;
    size_t length;
} tuple;

typedef struct call
{
    expression *callee;
    tuple arguments;
} call;

call call_create(expression *callee, tuple arguments);

typedef struct access_structure
{
    expression *object;
    expression *member;
} access_structure;

access_structure access_structure_create(expression *object,
                                         expression *member);

typedef struct match_case
{
    expression *key;
    expression *action;
} match_case;

match_case match_case_create(expression *key, expression *action);
void match_case_free(match_case *value);
int match_case_equals(match_case const left, match_case const right);

typedef struct match
{
    expression *input;
    match_case *cases;
    size_t number_of_cases;
} match;

match match_create(expression *input, match_case *cases,
                   size_t number_of_cases);

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

typedef struct declare
{
    expression *name;
    expression *type;
    expression *initializer;
} declare;

sequence sequence_create(expression *elements, size_t length);
void sequence_free(sequence const *value);
declare declare_create(expression *name, expression *type,
                       expression *initializer);
void declare_free(declare const *value);
tuple tuple_create(expression *elements, size_t length);
void tuple_free(tuple const *value);
expression expression_from_assign(assign value);
expression expression_from_return(expression *value);
expression expression_from_loop(sequence body);
expression expression_from_break(void);
expression expression_from_sequence(sequence value);
expression expression_from_access_structure(access_structure value);
expression expression_from_declare(declare value);
expression expression_from_match(match value);

struct expression
{
    expression_type type;
    union
    {
        lambda lambda;
        call call;
        integer integer_literal;
        access_structure access_structure;
        match match;
        unicode_string string;
        unicode_string identifier;
        expression *make_identifier;
        assign assign;
        expression *return_;
        sequence loop_body;
        sequence sequence;
        declare declare;
        tuple tuple;
    };
};

expression expression_from_integer_literal(integer value);
void call_free(call const *this);
void access_structure_free(access_structure const *this);
void match_free(match const *this);
expression expression_from_lambda(lambda lambda);
expression expression_from_unicode_string(unicode_string value);
expression expression_from_call(call value);
expression expression_from_identifier(unicode_string identifier);
expression expression_from_make_identifier(expression *value);
expression expression_from_tuple(tuple value);
expression *expression_allocate(expression value);
void expression_free(expression const *this);
int sequence_equals(sequence const left, sequence const right);
int declare_equals(declare const left, declare const right);
int assign_equals(assign const left, assign const right);
int match_equals(match const left, match const right);
int expression_equals(expression const *left, expression const *right);
