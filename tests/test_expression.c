#include "test_expression.h"
#include "lpg_expression.h"
#include "test.h"

static void test_equal(expression left, expression right)
{
    REQUIRE(expression_equals(&left, &right));
}

static void test_not_equal(expression left, expression right)
{
    REQUIRE(!expression_equals(&left, &right));
}

void test_expression(void)
{
    test_equal(expression_from_break(), expression_from_break());
    test_not_equal(expression_from_break(),
                   expression_from_integer_literal(integer_create(0, 0)));
    test_not_equal(expression_from_integer_literal(integer_create(0, 1)),
                   expression_from_integer_literal(integer_create(0, 0)));
}
