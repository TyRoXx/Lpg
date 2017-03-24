#pragma once
#include "lpg_expression.h"

typedef enum statement_type
{
    statement_assign,
    statement_return,
    statement_expression
} statement_type;

typedef struct assign
{
    expression *left;
    expression *right;
} assign;

assign assign_create(expression *left, expression *right);

typedef struct statement
{
    statement_type type;
    union
    {
        assign assign;
        expression *return_;
        expression *expression;
    };
} statement;

statement statement_from_assign(assign value);
statement statement_from_return(expression *value);
statement statement_from_expression(expression *value);
void statement_free(statement *s);
