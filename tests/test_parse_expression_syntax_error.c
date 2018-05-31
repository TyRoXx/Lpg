#include "test_parse_expression_syntax_error.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_array_size.h"
#include "lpg_allocate.h"
#include "handle_parse_error.h"

static void test_match_case(void);
static void test_return(void);
static void test_let(void);
static void test_function(void);
static void test_unnamed_function(void);
static void test_loops(void);
static void test_tokenizer_error(void);
static void test_assignment(void);

static void test_syntax_error(parse_error const *expected_errors, size_t const expected_count,
                              expression *const expected, unicode_string input)
{
    test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, expected_errors, expected_count};
    expression_parser parser = expression_parser_create(find_next_token, &user, handle_error, &user);
    expression_parser_result result = parse_expression(&parser, 0, 1);
    if (expected)
    {
        REQUIRE(result.is_success);
        REQUIRE(expression_equals(expected, &result.success));
        expression_free(expected);
        expression_free(&result.success);
        REQUIRE(user.base.remaining_size == 0);
        REQUIRE(!expression_parser_has_remaining_non_empty_tokens(&parser));
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
            parse_error_create(parse_error_integer_literal_out_of_range, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 39))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL,
                          unicode_string_from_c_str("340282366920938463463374607431768211456"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_integer_literal_out_of_range, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 95))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL,
                          unicode_string_from_c_str("3402823669209384634633746074317682114569"
                                                    "9999999999999999999999999999999999999999"
                                                    "999999999999999"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(1, 4))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("interface\n"
                                                                                              "    1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_parameter_list, source_location_create(1, 5))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("interface\n"
                                                                                              "    a "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(1, 6))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("interface\n"
                                                                                              "    a("));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(1, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("interface\n"
                                                                                              "    a():"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(2, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("interface\n"
                                                                                              "    a(): unit\n"
                                                                                              "    b():"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(1, 4))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let s = struct\n"
                                                                                              "    1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_colon, source_location_create(1, 5))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let s = struct\n"
                                                                                              "    a "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(1, 6))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let s = struct\n"
                                                                                              "    a:1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(1, 7))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let s = struct\n"
                                                                                              "    a: "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let s = impl\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 4)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 0))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 5))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl i"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_for, source_location_create(0, 7))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl i tt"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl i for"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_newline, source_location_create(0, 12))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("impl i for x"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_left_parenthesis, source_location_create(0, 7))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("type-of "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("type-of("));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_right_parenthesis, source_location_create(0, 9))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("type-of(0"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("import"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(0, 7))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("import "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_right_bracket, source_location_create(0, 5))};
        expression expected = expression_from_enum(
            enum_expression_create(source_location_create(0, 0), generic_parameter_list_create(NULL, 0), NULL, 0));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("enum["));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_right_bracket, source_location_create(0, 7))};
        unicode_string *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = unicode_string_from_c_str("a");
        expression expected = expression_from_enum(enum_expression_create(
            source_location_create(0, 0), generic_parameter_list_create(parameters, 1), NULL, 0));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("enum[a,"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_right_bracket, source_location_create(0, 6))};
        unicode_string *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = unicode_string_from_c_str("a");
        expression expected = expression_from_enum(enum_expression_create(
            source_location_create(0, 0), generic_parameter_list_create(parameters, 1), NULL, 0));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("enum[a"));
    }

    test_tokenizer_error();
    test_unnamed_function();
    test_function();
    test_loops();
    test_assignment();
    test_let();
    test_match_case();
    test_return();
}

static void test_assignment(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 4))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 4)))),
            equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a ==1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 3)))),
            equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a==1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 3)))),
            not_equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a!=1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 3)))),
            less_than_or_equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a<=1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 3))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 3)))),
            greater_than_or_equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a>=1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 2))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)))),
            less_than));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a<1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 2))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)))),
            greater_than));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a>1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 1))};
        expression expected = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(
                identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)))),
            expression_allocate(expression_from_integer_literal(
                integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 4)))),
            equals));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a== 1"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 5)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 6))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 0)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("a == ?"));
    }
}

static void test_tokenizer_error(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 1))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("?"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 1)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("?a"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0))};
        expression expected = expression_from_break(source_location_create(0, 1));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("?break"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 1)));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("1a"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0)),
            parse_error_create(parse_error_invalid_token, source_location_create(0, 1)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("1?"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 0))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 5)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("12345a"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_element_name, source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("a.."));
    }
}

static void test_unnamed_function(void)
{
    {

        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(0, 3)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("=> a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("=>"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 4))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("=>\n    "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 2))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(1, 3)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("\n=> a"));
    }
    {

        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 4)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 6))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(1, 7)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("\n    => a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(1, 2)),
            parse_error_create(parse_error_expected_expression, source_location_create(2, 0)),
            parse_error_create(parse_error_expected_expression, source_location_create(2, 2))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("a"), source_location_create(2, 3)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("=>\n"
                                                                                                   "=>\n"
                                                                                                   "=> a"));
    }
}

static void test_function(void)
{
    {
        parse_error const expected_error =
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2));
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 3)));
        expression expected = expression_from_call(
            call_create(expression_allocate(expression_from_identifier(identifier_expression_create(
                            unicode_string_from_c_str("f"), source_location_create(0, 0)))),
                        tuple_create(arguments, 1, source_location_create(0, 0)), source_location_create(0, 4)));
        test_syntax_error(&expected_error, 1, &expected, unicode_string_from_c_str("f(,1)"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 4)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 4))};
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)));
        expression expected = expression_from_call(
            call_create(expression_allocate(expression_from_identifier(identifier_expression_create(
                            unicode_string_from_c_str("f"), source_location_create(0, 0)))),
                        tuple_create(arguments, 1, source_location_create(0, 0)), source_location_create(0, 4)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("f(1,)"));
    }
    {
        parse_error const expected_error =
            parse_error_create(parse_error_expected_arguments, source_location_create(0, 2));
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(&expected_error, 1, &expected, unicode_string_from_c_str("f("));
    }
    {
        parse_error const expected_error =
            parse_error_create(parse_error_expected_arguments, source_location_create(0, 4));
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(&expected_error, 1, &expected, unicode_string_from_c_str("f(1,"));
    }
    {
        parse_error const expected_error = parse_error_create(parse_error_expected_space, source_location_create(0, 4));
        expression *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)));
        arguments[1] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 4)));
        expression expected = expression_from_call(
            call_create(expression_allocate(expression_from_identifier(identifier_expression_create(
                            unicode_string_from_c_str("f"), source_location_create(0, 0)))),
                        tuple_create(arguments, 2, source_location_create(0, 0)), source_location_create(0, 5)));
        test_syntax_error(&expected_error, 1, &expected, unicode_string_from_c_str("f(1,2)"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 2)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 3)),
            parse_error_create(parse_error_expected_arguments, source_location_create(0, 3))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("f(,"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 4)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 4)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 5)),
            parse_error_create(parse_error_expected_arguments, source_location_create(0, 5))};
        expression expected = expression_from_identifier(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 0)));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("f(1,,"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 11)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 12))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = () ?"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_lambda_body, source_location_create(0, 16))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (): unit 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_lambda_body, source_location_create(0, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = ()1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(0, 17))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (a: unit,) 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_comma, source_location_create(0, 16))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (a: unit ) 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(1, 4))};
        expression expected = expression_from_declare(declare_create(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 4)), NULL,
            expression_allocate(expression_from_lambda(
                lambda_create(generic_parameter_list_create(NULL, 0),
                              function_header_tree_create(
                                  NULL, 0, expression_allocate(expression_from_identifier(identifier_expression_create(
                                               unicode_string_from_c_str("unit"), source_location_create(0, 12))))),
                              expression_allocate(expression_from_sequence(sequence_create(NULL, 0))),
                              source_location_create(0, 8))))));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("let f = (): unit\n"
                                                                                                   "    ?"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_identifier, source_location_create(0, 18))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL,
                          unicode_string_from_c_str("let f = (a: unit, 1) 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_colon, source_location_create(0, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (a ) 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 11))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (a:) 1"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 12)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 13)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 14))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = (a: ?)"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 12)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 13))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = {a, }"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 9))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = !"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 12))};
        expression expected = expression_from_declare(declare_create(
            identifier_expression_create(unicode_string_from_c_str("f"), source_location_create(0, 4)), NULL,
            expression_allocate(expression_from_binary_operator(binary_operator_expression_create(
                expression_allocate(expression_from_integer_literal(
                    integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 8)))),
                expression_allocate(expression_from_integer_literal(
                    integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 11)))),
                equals)))));
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected, unicode_string_from_c_str("let f = 1 ==2"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_declaration_or_assignment, source_location_create(0, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f : 1 == 2 = 3"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 8)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let f = !="));
    }
}

static void test_let(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 14)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 15))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let a : int = ?"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 4)),
            parse_error_create(parse_error_expected_identifier, source_location_create(0, 5))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let ?"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 3)),
            parse_error_create(parse_error_expected_space, source_location_create(0, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let?"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_invalid_token, source_location_create(0, 8)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 9)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 10)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 11)),
            parse_error_create(parse_error_expected_declaration_or_assignment, source_location_create(0, 13))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let a : ? = 1"));
    }
}

static void test_return(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("return"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 7))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("return "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 7)),
            parse_error_create(parse_error_expected_expression, source_location_create(0, 13))};
        expression expected = expression_from_return(expression_allocate(expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 123), source_location_create(0, 14)))));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), &expected,
                          unicode_string_from_c_str("return return 123"));
    }
}

static void test_match_case(void)
{
    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("let a : "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(0, 5))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match:a\n"
                                                                                              "    case 1: 2\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_newline, source_location_create(0, 7))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a:\n"
                                                                                              "    case 1: 2\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_case, source_location_create(1, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                                            "    \n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(0, 6))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_space, source_location_create(1, 8))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(2, 9))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 0: 2\n"
                                                                                              "    case "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_colon, source_location_create(1, 10))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 1\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(2, 0))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 1:\n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(1, 12))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 1: "));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_expression, source_location_create(1, 12)),
            parse_error_create(parse_error_expected_expression, source_location_create(2, 0))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 1: \n"));
    }

    {
        parse_error const expected_errors[] = {
            parse_error_create(parse_error_expected_newline, source_location_create(1, 13))};
        test_syntax_error(
            expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("match a\n"
                                                                                              "    case 1: 2"));
    }
}

static void test_loops(void)
{
    parse_error const expected_errors[] = {
        parse_error_create(parse_error_expected_newline, source_location_create(0, 4))};
    test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors), NULL, unicode_string_from_c_str("loop"));
}
