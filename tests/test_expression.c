#include "test_expression.h"
#include "lpg_allocate.h"
#include "lpg_expression_pool.h"
#include "test.h"
#include <lpg_type.h>

static void test_equal(expression left, expression right)
{
    REQUIRE(expression_equals(&left, &right));
    expression_free(left);
    expression_free(right);
}

static void test_not_equal(expression left, expression right)
{
    REQUIRE(!expression_equals(&left, &right));
    expression_free(left);
    expression_free(right);
}

static void test_expression_pool(void)
{
    expression_pool pool1 = expression_pool_create();
    expression_pool pool2 = expression_pool_create();
    expression *allocated1[10000];
    expression *allocated2[10000];
    for (size_t i = 0; i < LPG_ARRAY_SIZE(allocated1); ++i)
    {
        allocated1[i] = expression_pool_allocate(&pool1);
        *allocated1[i] = expression_from_break(break_expression_create(source_location_create(1, 1), NULL));
        allocated2[i] = expression_pool_allocate(&pool2);
        *allocated2[i] = expression_from_break(break_expression_create(source_location_create(1, 1), NULL));
    }
    for (size_t i = 0; i < LPG_ARRAY_SIZE(allocated1); ++i)
    {
        REQUIRE(expression_is_valid(*allocated1[i]));
        REQUIRE(allocated1[i]->type == expression_type_break);
        REQUIRE(expression_is_valid(*allocated2[i]));
        REQUIRE(allocated2[i]->type == expression_type_break);
    }
    expression_pool_free(pool1);
    expression_pool_free(pool2);
}

void test_expression(void)
{
    test_expression_pool();
    expression_pool pool = expression_pool_create();
    test_equal(expression_from_break(break_expression_create(source_location_create(0, 0), NULL)),
               expression_from_break(break_expression_create(source_location_create(0, 0), NULL)));
    test_not_equal(expression_from_break(break_expression_create(source_location_create(0, 0), NULL)),
                   expression_from_break(break_expression_create(source_location_create(0, 1), NULL)));
    test_not_equal(expression_from_break(break_expression_create(source_location_create(0, 0), NULL)),
                   expression_from_integer_literal(
                       integer_literal_expression_create(integer_create(0, 0), source_location_create(0, 0))));
    test_not_equal(expression_from_integer_literal(
                       integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 0))),
                   expression_from_integer_literal(
                       integer_literal_expression_create(integer_create(0, 0), source_location_create(0, 0))));
    test_not_equal(expression_from_call(
                       call_create(expression_allocate(expression_from_identifier(identifier_expression_create(
                                                           unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                                       &pool),
                                   tuple_create(NULL, 0, source_location_create(0, 0)), source_location_create(0, 0))),
                   expression_from_call(
                       call_create(expression_allocate(expression_from_identifier(identifier_expression_create(
                                                           unicode_view_from_c_str("g"), source_location_create(0, 0))),
                                                       &pool),
                                   tuple_create(NULL, 0, source_location_create(0, 0)), source_location_create(0, 0))));
    {
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        test_not_equal(expression_from_call(call_create(
                           expression_allocate(expression_from_identifier(identifier_expression_create(
                                                   unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                               &pool),
                           tuple_create(NULL, 0, source_location_create(0, 0)), source_location_create(0, 0))),
                       expression_from_call(call_create(
                           expression_allocate(expression_from_identifier(identifier_expression_create(
                                                   unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                               &pool),
                           tuple_create(arguments, 1, source_location_create(0, 0)), source_location_create(0, 0))));
    }
    {
        expression *arguments_left = allocate_array(1, sizeof(*arguments_left));
        arguments_left[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        expression *arguments_right = allocate_array(1, sizeof(*arguments_right));
        arguments_right[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 0)));
        test_not_equal(
            expression_from_call(call_create(
                expression_allocate(expression_from_identifier(identifier_expression_create(
                                        unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                    &pool),
                tuple_create(arguments_left, 1, source_location_create(0, 0)), source_location_create(0, 0))),
            expression_from_call(call_create(
                expression_allocate(expression_from_identifier(identifier_expression_create(
                                        unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                    &pool),
                tuple_create(arguments_right, 1, source_location_create(0, 0)), source_location_create(0, 0))));
    }
    {
        expression *elements_left = allocate_array(1, sizeof(*elements_left));
        elements_left[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        test_not_equal(expression_from_sequence(sequence_create(elements_left, 1, source_location_create(0, 0))),
                       expression_from_sequence(sequence_create(NULL, 0, source_location_create(0, 0))));
    }
    {
        expression *elements_left = allocate_array(1, sizeof(*elements_left));
        elements_left[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        expression *elements_right = allocate_array(1, sizeof(*elements_right));
        elements_right[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 0)));
        test_not_equal(expression_from_sequence(sequence_create(elements_left, 1, source_location_create(0, 0))),
                       expression_from_sequence(sequence_create(elements_right, 1, source_location_create(0, 0))));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        match_case *right = allocate_array(2, sizeof(*right));
        right[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        right[1] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        test_not_equal(
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
                left, 1)),
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
                right, 2)));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        match_case *right = allocate_array(1, sizeof(*right));
        right[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                    integer_create(0, 1), source_location_create(0, 0))),
                                &pool));
        test_not_equal(
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
                left, 1)),
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
                right, 1)));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        match_case *right = allocate_array(1, sizeof(*right));
        right[0] = match_case_create(
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
            expression_allocate(
                expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool));
        test_not_equal(
            expression_from_match(
                match_create(source_location_create(0, 0),
                             expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                     integer_create(1, 2), source_location_create(0, 0))),
                                                 &pool),
                             left, 1)),
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), &pool),
                right, 1)));
        expression_pool_free(pool);
    }
}
