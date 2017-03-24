#include "lpg_statement.h"
#include "lpg_allocate.h"
#include "lpg_for.h"

assign assign_create(expression *left, expression *right)
{
    assign result = {left, right};
    return result;
}

sequence sequence_create(statement *elements, size_t length)
{
    sequence result = {elements, length};
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

statement statement_from_loop(statement *body)
{
    statement result;
    result.type = statement_loop;
    result.loop_body = body;
    return result;
}

statement statement_from_break()
{
    statement result;
    result.type = statement_break;
    return result;
}

statement statement_from_sequence(sequence value)
{
    statement result;
    result.type = statement_sequence;
    result.sequence = value;
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

    case statement_loop:
        statement_deallocate(s->loop_body);
        break;

    case statement_break:
        break;

    case statement_sequence:
        LPG_FOR(size_t, i, s->sequence.length)
        {
            statement_free(s->sequence.elements + i);
        }
        deallocate(s->sequence.elements);
        break;
    }
}

statement *statement_allocate(statement value)
{
    statement *result = allocate(sizeof(*result));
    *result = value;
    return result;
}

void statement_deallocate(statement *s)
{
    statement_free(s);
    deallocate(s);
}
