#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"

static void sequence_element(expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
    case expression_type_call:
    case expression_type_integer_literal:
    case expression_type_access_structure:
    case expression_type_match:
    case expression_type_string:
    case expression_type_identifier:
    case expression_type_make_identifier:
    case expression_type_assign:
    case expression_type_return:
    case expression_type_loop:
    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        UNREACHABLE();
    }
}

static void function(sequence const body)
{
    LPG_FOR(size_t, i, body.length)
    {
        sequence_element(body.elements[i]);
    }
}

void check(sequence const root)
{
    function(root);
}
