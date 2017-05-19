#include "test_parse_expression_success.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"
#include "lpg_save_expression.h"
#include "handle_parse_error.h"
#include "lpg_find_next_token.h"

static void test_successful_parse(expression expected, unicode_string input)
{
    test_parser_user user = {
        {input.data, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser, 0, 1);
    REQUIRE(result.is_success);
    REQUIRE(user.base.remaining_size == 0);
    REQUIRE(expression_equals(&expected, &result.success));
    expression_free(&expected);
    expression_free(&result.success);
    unicode_string_free(&input);
}

void test_parse_expression_success(void)
{
    test_successful_parse(expression_from_break(source_location_create(0, 0)),
                          unicode_string_from_c_str("break"));

    test_successful_parse(
        expression_from_return(expression_allocate(
            expression_from_integer_literal(integer_create(0, 123)))),
        unicode_string_from_c_str("return 123"));

    test_successful_parse(
        expression_from_return(expression_allocate(expression_from_call(
            call_create(expression_allocate(expression_from_identifier(
                            identifier_expression_create(
                                unicode_string_from_c_str("f"),
                                source_location_create(0, 7)))),
                        tuple_create(NULL, 0), source_location_create(0, 9))))),
        unicode_string_from_c_str("return f()"));

    test_successful_parse(
        expression_from_identifier(identifier_expression_create(
            unicode_string_from_c_str("a"), source_location_create(0, 0))),
        unicode_string_from_c_str("a"));

    test_successful_parse(
        expression_from_integer_literal(integer_create(0, 123)),
        unicode_string_from_c_str("123"));

    {
        tuple arguments = tuple_create(NULL, 0);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                arguments, source_location_create(0, 2))),
            unicode_string_from_c_str("f()"));
    }
    {
        tuple arguments = tuple_create(NULL, 0);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(expression_from_call(call_create(
                    expression_allocate(
                        expression_from_identifier(identifier_expression_create(
                            unicode_string_from_c_str("f"),
                            source_location_create(0, 0)))),
                    arguments, source_location_create(0, 2)))),
                arguments, source_location_create(0, 4))),
            unicode_string_from_c_str("f()()"));
    }
    {
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        tuple arguments_tuple = tuple_create(arguments, 1);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                arguments_tuple, source_location_create(0, 3))),
            unicode_string_from_c_str("f(1)"));
    }
    {
        expression *arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        arguments[1] = expression_from_integer_literal(integer_create(0, 2));
        tuple arguments_tuple = tuple_create(arguments, 2);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("f"),
                        source_location_create(0, 0)))),
                arguments_tuple, source_location_create(0, 6))),
            unicode_string_from_c_str("f(1, 2)"));
    }
    {
        expression *elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_break(source_location_create(1, 4));
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 1)),
            unicode_string_from_c_str("loop\n"
                                      "    break"));
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expression_from_identifier(identifier_expression_create(
            unicode_string_from_c_str("f"), source_location_create(1, 4)));
        elements[1] = expression_from_break(source_location_create(2, 4));
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    f\n"
                                      "    break"));
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expression_from_identifier(identifier_expression_create(
            unicode_string_from_c_str("f"), source_location_create(1, 4)));
        elements[1] = expression_from_break(source_location_create(2, 4));
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    f\n"
                                      "    break\n"));
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(source_location_create(2, 8));
        expression *outer_loop = allocate_array(1, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1));
        test_successful_parse(
            expression_from_loop(sequence_create(outer_loop, 1)),
            unicode_string_from_c_str("loop\n"
                                      "    loop\n"
                                      "        break\n"));
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(source_location_create(2, 8));
        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1));
        outer_loop[1] = expression_from_break(source_location_create(3, 4));
        test_successful_parse(
            expression_from_loop(sequence_create(outer_loop, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    loop\n"
                                      "        break\n"
                                      "    break\n"));
    }

    test_successful_parse(
        expression_from_assign(assign_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))))),
        unicode_string_from_c_str("a = 1"));

    test_successful_parse(
        expression_from_declare(declare_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("int"),
                                             source_location_create(0, 4)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))))),
        unicode_string_from_c_str("a : int = 1"));

    test_successful_parse(
        expression_from_match(match_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 6)))),
            NULL, 0)),
        unicode_string_from_c_str("match a\n"));

    {
        match_case *const cases = allocate_array(1, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 2))));
        test_successful_parse(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("a"),
                        source_location_create(0, 6)))),
                cases, 1)),
            unicode_string_from_c_str("match a\n"
                                      "    case 1: 2\n"));
    }

    {
        match_case *const cases = allocate_array(2, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 2))));
        cases[1] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 3))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 4))));
        test_successful_parse(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("a"),
                        source_location_create(0, 6)))),
                cases, 2)),
            unicode_string_from_c_str("match a\n"
                                      "    case 1: 2\n"
                                      "    case 3: 4\n"));
    }

    {
        match_case *const cases = allocate_array(2, sizeof(*cases));
        expression *sequence = allocate_array(1, sizeof(*sequence));
        sequence[0] = expression_from_integer_literal(integer_create(0, 2));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))),
            expression_allocate(
                expression_from_sequence(sequence_create(sequence, 1))));
        cases[1] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 3))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 4))));
        test_successful_parse(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_identifier(identifier_expression_create(
                        unicode_string_from_c_str("a"),
                        source_location_create(0, 6)))),
                cases, 2)),
            unicode_string_from_c_str("match a\n"
                                      "    case 1:\n"
                                      "        2\n"
                                      "    case 3: 4\n"));
    }
}
