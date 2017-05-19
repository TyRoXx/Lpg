#include "test_parse_expression_syntax_error.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_array_size.h"
#include "lpg_allocate.h"
#include "handle_parse_error.h"
#include "lpg_find_next_token.h"

static void test_syntax_error(parse_error const *expected_errors,
                              size_t const expected_count,
                              expression *const expected, unicode_string input)
{
    test_parser_user user = {
        {input.data, input.length, source_location_create(0, 0)},
        expected_errors,
        expected_count};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser, 0, 1);
    if (expected)
    {
        REQUIRE(result.is_success);
        REQUIRE(expression_equals(expected, &result.success));
        expression_free(expected);
        expression_free(&result.success);
        REQUIRE(user.base.remaining_size == 0);
    }
    else
    {
        REQUIRE(!result.is_success);
    }
    unicode_string_free(&input);
    REQUIRE(user.expected_count == 0);
}

void test_parse_expression_syntax_error(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 1))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("?"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_invalid_token, source_location_create(0, 0))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 1)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("?a"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_invalid_token, source_location_create(0, 0))};
        expression expected =
            expression_from_break(source_location_create(0, 1));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("?break"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_invalid_token, source_location_create(0, 0))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 1)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("1a"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 0)),
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 1)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("1?"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_invalid_token, source_location_create(0, 0))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 5)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("12345a"));
    }

    {

        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 3)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("=> a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("=>"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("=>\n    "));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_integer_literal_out_of_range,
                               source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression,
                               source_location_create(0, 39))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str(
                                    "340282366920938463463374607431768211456"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_integer_literal_out_of_range,
                               source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression,
                               source_location_create(0, 95))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL,
            unicode_string_from_c_str("3402823669209384634633746074317682114569"
                                      "9999999999999999999999999999999999999999"
                                      "999999999999999"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 2))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(1, 3)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("\n=> a"));
    }
    {

        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 6))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(1, 7)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("\n    => a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 2)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(2, 0)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(2, 2))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(2, 3)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("=>\n"
                                                               "=>\n"
                                                               "=> a"));
    }
    {
        parse_error const expected_error = parse_error_create(
            parse_error_expected_expression, source_location_create(0, 2));
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        expression expected = expression_from_call(call_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("f"),
                                             source_location_create(0, 0)))),
            tuple_create(arguments, 1), source_location_create(0, 4)));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("f(,1)"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_space, source_location_create(0, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 4))};
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        expression expected = expression_from_call(call_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("f"),
                                             source_location_create(0, 0)))),
            tuple_create(arguments, 1), source_location_create(0, 4)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("f(1,)"));
    }
    {
        parse_error const expected_error = parse_error_create(
            parse_error_expected_arguments, source_location_create(0, 2));
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("f("));
    }
    {
        parse_error const expected_error = parse_error_create(
            parse_error_expected_arguments, source_location_create(0, 4));
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("f(1,"));
    }
    {
        parse_error const expected_error = parse_error_create(
            parse_error_expected_space, source_location_create(0, 4));
        expression *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        arguments[1] = expression_from_integer_literal(integer_create(0, 2));
        expression expected = expression_from_call(call_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("f"),
                                             source_location_create(0, 0)))),
            tuple_create(arguments, 2), source_location_create(0, 5)));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("f(1,2)"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 3)),
            parse_error_create(
                parse_error_expected_arguments, source_location_create(0, 3))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("f(,"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_space, source_location_create(0, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 5)),
            parse_error_create(
                parse_error_expected_arguments, source_location_create(0, 5))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("f(1,,"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_newline, source_location_create(0, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("loop"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_assign(assign_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a =1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(
                parse_error_expected_space, source_location_create(0, 2))};
        expression expected = expression_from_assign(assign_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a=1"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 1))};
        expression expected = expression_from_assign(assign_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a= 1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_declaration_or_assignment,
                               source_location_create(0, 2))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 0)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a "));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 5))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 0)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a = ?"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 1))};
        expression expected = expression_from_declare(declare_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("int"),
                                             source_location_create(0, 3)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a: int = 1"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_declare(declare_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("int"),
                                             source_location_create(0, 3)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a :int = 1"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 7))};
        expression expected = expression_from_declare(declare_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("int"),
                                             source_location_create(0, 4)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a : int= 1"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 9))};
        expression expected = expression_from_declare(declare_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 0)))),
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("int"),
                                             source_location_create(0, 4)))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a : int =1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 10)),
            parse_error_create(parse_error_expected_expression,
                               source_location_create(0, 11))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("a : int = ?"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_invalid_token, source_location_create(0, 4)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 5)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 6)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 7)),
            parse_error_create(
                parse_error_expected_assignment, source_location_create(0, 9)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 9))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("a : ? = 1"));
    }
    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(0, 4))};
        expression expected =
            expression_from_identifier(identifier_expression_create(
                unicode_string_from_c_str("a"), source_location_create(0, 0)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("a : "));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 5))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match:a\n"
                                                          "    case 1: 2\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_newline, source_location_create(0, 7))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a:\n"
                                                          "    case 1: 2\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_case, source_location_create(1, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    \n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match "));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(1, 8))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(2, 9))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 0: 2\n"
                                                          "    case "));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_colon, source_location_create(1, 10))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 1\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(2, 0))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 1:\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(1, 12))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 1: "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(1, 12)),
            parse_error_create(
                parse_error_expected_expression, source_location_create(2, 0))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 1: \n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_newline, source_location_create(1, 13))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("match a\n"
                                                          "    case 1: 2"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(1, 11))};
        match_case *const cases = allocate_array(1, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 2))));
        expression expected = expression_from_match(match_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"),
                                             source_location_create(0, 6)))),
            cases, 1));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected,
                          unicode_string_from_c_str("match a\n"
                                                    "    case 1:2\n"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_space, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("return"));
    }

    {
        parse_error const expected_errors[] = {parse_error_create(
            parse_error_expected_expression, source_location_create(0, 7))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("return "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(
                parse_error_expected_expression, source_location_create(0, 7)),
            parse_error_create(parse_error_expected_expression,
                               source_location_create(0, 13))};
        expression expected = expression_from_return(expression_allocate(
            expression_from_integer_literal(integer_create(0, 123))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected,
                          unicode_string_from_c_str("return return 123"));
    }
}
