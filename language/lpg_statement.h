#pragma once
#include "lpg_expression.h"

typedef enum statement_type
{
    statement_assign,
    statement_return,
    statement_expression,
    statement_loop,
    statement_break,
    statement_sequence
} statement_type;

typedef struct assign
{
    expression *left;
    expression *right;
} assign;

assign assign_create(expression *left, expression *right);

typedef struct sequence
{
    struct statement *elements;
    size_t length;
} sequence;

typedef struct statement
{
    statement_type type;
    union
    {
        assign assign;
        expression *return_;
        expression *expression;
        struct statement *loop_body;
        sequence sequence;
    };
} statement;

sequence sequence_create(statement *elements, size_t length);

statement statement_from_assign(assign value);
statement statement_from_return(expression *value);
statement statement_from_expression(expression *value);
statement statement_from_loop(statement *body);
statement statement_from_break(void);
statement statement_from_sequence(sequence value);
void statement_free(statement *s);
statement *statement_allocate(statement value);
void statement_deallocate(statement *s);
