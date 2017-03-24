#include "lpg_statement.h"

assign assign_create(expression *left, expression *right)
{
    assign result = {left, right};
    return result;
}

statement statement_from_assign(assign value)
{
    statement result;
    result.type = statement_assign;
    result.assign = value;
    return result;
}

statement statement_from_return(expression *value)
{
    statement result;
    result.type = statement_return;
    result.return_ = value;
    return result;
}

statement statement_from_expression(expression *value)
{
    statement result;
    result.type = statement_expression;
    result.expression = value;
    return result;
}

void statement_free(statement *s)
{
    switch (s->type)
    {
    case statement_assign:
        expression_deallocate(s->assign.left);
        expression_deallocate(s->assign.right);
        break;

    case statement_return:
        expression_deallocate(s->return_);
        break;

    case statement_expression:
        expression_deallocate(s->expression);
        break;
    }
}
