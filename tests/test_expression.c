#include <lpg_type.h>
#include "test_expression.h"
#include "lpg_expression.h"
#include "test.h"
#include "lpg_allocate.h"

static void test_equal(expression left, expression right)
{
    REQUIRE(expression_equals(&left, &right));
    expression_free(&left);
    expression_free(&right);
}

static void test_not_equal(expression left, expression right)
{
    REQUIRE(!expression_equals(&left, &right));
    expression_free(&left);
    expression_free(&right);
}

void test_expression(void)
{
    test_equal(expression_from_break(source_location_create(0, 0)),
               expression_from_break(source_location_create(0, 0)));
    test_not_equal(expression_from_break(source_location_create(0, 0)),
                   expression_from_break(source_location_create(0, 1)));
    test_not_equal(
        expression_from_break(source_location_create(0, 0)),
        expression_from_integer_literal(integer_literal_expression_create(
            integer_create(0, 0), source_location_create(0, 0))));
    test_not_equal(
        expression_from_integer_literal(integer_literal_expression_create(
            integer_create(0, 1), source_location_create(0, 0))),
        expression_from_integer_literal(integer_literal_expression_create(
            integer_create(0, 0), source_location_create(0, 0))));
    test_not_equal(
        expression_from_call(call_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("f"),
                                             source_location_create(0, 0)))),
            tuple_create(NULL, 0), source_location_create(0, 0))),
        expression_from_call(call_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("g"),
                                             source_location_create(0, 0)))),
            tuple_create(NULL, 0), source_location_create(0, 0))));
    {
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_break(source_location_create(0, 0));
        test_not_equal(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                tuple_create(NULL, 0), source_location_create(0, 0))),
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                tuple_create(arguments, 1), source_location_create(0, 0))));
    }
    {
        expression *arguments_left = allocate_array(1, sizeof(*arguments_left));
        arguments_left[0] = expression_from_break(source_location_create(0, 0));
        expression *arguments_right =
            allocate_array(1, sizeof(*arguments_right));
        arguments_right[0] =
            expression_from_integer_literal(integer_literal_expression_create(
                integer_create(0, 1), source_location_create(0, 0)));
        test_not_equal(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                tuple_create(arguments_left, 1), source_location_create(0, 0))),
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                tuple_create(arguments_right, 1),
                source_location_create(0, 0))));
    }
    {
        expression *elements_left = allocate_array(1, sizeof(*elements_left));
        elements_left[0] = expression_from_break(source_location_create(0, 0));
        test_not_equal(
            expression_from_sequence(sequence_create(elements_left, 1)),
            expression_from_sequence(sequence_create(NULL, 0)));
    }
    {
        expression *elements_left = allocate_array(1, sizeof(*elements_left));
        elements_left[0] = expression_from_break(source_location_create(0, 0));
        expression *elements_right = allocate_array(1, sizeof(*elements_right));
        elements_right[0] =
            expression_from_integer_literal(integer_literal_expression_create(
                integer_create(0, 1), source_location_create(0, 0)));
        test_not_equal(
            expression_from_sequence(sequence_create(elements_left, 1)),
            expression_from_sequence(sequence_create(elements_right, 1)));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(expression_allocate(expression_from_break(
                                        source_location_create(0, 0))),
                                    expression_allocate(expression_from_break(
                                        source_location_create(0, 0))));
        match_case *right = allocate_array(2, sizeof(*right));
        right[0] = match_case_create(expression_allocate(expression_from_break(
                                         source_location_create(0, 0))),
                                     expression_allocate(expression_from_break(
                                         source_location_create(0, 0))));
        right[1] = match_case_create(expression_allocate(expression_from_break(
                                         source_location_create(0, 0))),
                                     expression_allocate(expression_from_break(
                                         source_location_create(0, 0))));
        test_not_equal(expression_from_match(match_create(
                           source_location_create(0, 0),
                           expression_allocate(expression_from_break(
                               source_location_create(0, 0))),
                           left, 1)),
                       expression_from_match(match_create(
                           source_location_create(0, 0),
                           expression_allocate(expression_from_break(
                               source_location_create(0, 0))),
                           right, 2)));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(expression_allocate(expression_from_break(
                                        source_location_create(0, 0))),
                                    expression_allocate(expression_from_break(
                                        source_location_create(0, 0))));
        match_case *right = allocate_array(1, sizeof(*right));
        right[0] = match_case_create(
            expression_allocate(
                expression_from_break(source_location_create(0, 0))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(
                    integer_create(0, 1), source_location_create(0, 0)))));
        test_not_equal(expression_from_match(match_create(
                           source_location_create(0, 0),
                           expression_allocate(expression_from_break(
                               source_location_create(0, 0))),
                           left, 1)),
                       expression_from_match(match_create(
                           source_location_create(0, 0),
                           expression_allocate(expression_from_break(
                               source_location_create(0, 0))),
                           right, 1)));
    }
    {
        match_case *left = allocate_array(1, sizeof(*left));
        left[0] = match_case_create(expression_allocate(expression_from_break(
                                        source_location_create(0, 0))),
                                    expression_allocate(expression_from_break(
                                        source_location_create(0, 0))));
        match_case *right = allocate_array(1, sizeof(*right));
        right[0] = match_case_create(expression_allocate(expression_from_break(
                                         source_location_create(0, 0))),
                                     expression_allocate(expression_from_break(
                                         source_location_create(0, 0))));
        test_not_equal(
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(expression_from_integer_literal(
                    integer_literal_expression_create(
                        integer_create(1, 2), source_location_create(0, 0)))),
                left, 1)),
            expression_from_match(match_create(
                source_location_create(0, 0),
                expression_allocate(
                    expression_from_break(source_location_create(0, 0))),
                right, 1)));
    }
}
