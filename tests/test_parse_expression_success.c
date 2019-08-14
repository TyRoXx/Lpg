#include "test_parse_expression_success.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_parse_expression.h"
#include "lpg_save_expression.h"
#include "test.h"

static void test_loops(void);

static void test_comment(void);

static void test_function_calls(void);

static void test_match_cases(void);

static void test_comparison(void);

static void test_assignments(void);

static void test_lambdas(void);

static void test_new_lines(void);

static void test_successful_parse_impl(expression const expected, unicode_string const input, bool const is_statement,
                                       expression_pool *const pool)
{
    test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(&user.base, handle_error, &user, pool);
    expression_parser_result result = parse_expression(&parser, 0, is_statement);
    REQUIRE(result.is_success);
    REQUIRE(user.base.remaining_size == 0);
    REQUIRE(expression_equals(&expected, &result.success));
    REQUIRE(!expression_parser_has_remaining_non_empty_tokens(&parser));
    expression_parser_free(parser);
    expression_free(result.success);
    unicode_string_free(&input);
}

static void test_save_expression_roundtrip(expression const tree, bool const is_statement, expression_pool *const pool)
{
    memory_writer buffer = {NULL, 0, 0};
    whitespace_state const whitespace = {0, false};
    REQUIRE(success_yes == save_expression(memory_writer_erase(&buffer), &tree, whitespace));
    test_successful_parse_impl(tree, unicode_string_from_range(buffer.data, buffer.used), is_statement, pool);
    memory_writer_free(&buffer);
}

static void test_successful_parse(expression const expected, unicode_string const input, bool const is_statement,
                                  expression_pool *const pool)
{
    test_successful_parse_impl(expected, input, is_statement, pool);
    test_save_expression_roundtrip(expected, is_statement, pool);
    expression_free(expected);
}

void test_parse_expression_success(void)
{
    expression_pool pool = expression_pool_create();

    test_successful_parse(expression_from_break(break_expression_create(source_location_create(0, 0), NULL)),
                          unicode_string_from_c_str("break"), true, &pool);

    test_successful_parse(
        expression_from_return(expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                       integer_create(0, 123), source_location_create(0, 7))),
                                                   &pool)),
        unicode_string_from_c_str("return 123"), true, &pool);

    test_successful_parse(expression_from_return(expression_allocate(
                              expression_from_call(call_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("f"), source_location_create(0, 7))),
                                                      &pool),
                                  tuple_create(NULL, 0, source_location_create(0, 0)), source_location_create(0, 9))),
                              &pool)),
                          unicode_string_from_c_str("return f()"), true, &pool);

    test_successful_parse(expression_from_identifier(
                              identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 0))),
                          unicode_string_from_c_str("a"), false, &pool);

    test_successful_parse(expression_from_integer_literal(
                              integer_literal_expression_create(integer_create(0, 123), source_location_create(0, 0))),
                          unicode_string_from_c_str("123"), false, &pool);

    test_successful_parse(expression_from_string(string_expression_create(
                              unicode_view_from_c_str("\"test\""), source_location_create(0, 0))),
                          unicode_string_from_c_str("\"test\""), false, &pool);

    test_successful_parse(expression_from_not(not_expression_create(
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(0, 1))),
                                                  &pool))),
                          unicode_string_from_c_str("!1"), true, &pool);

    test_successful_parse(expression_from_interface(interface_expression_create(
                              generic_parameter_list_create(NULL, 0), source_location_create(0, 0), NULL, 0)),
                          unicode_string_from_c_str("interface"), false, &pool);

    {
        interface_expression_method *const methods = allocate_array(1, sizeof(*methods));
        methods[0] = interface_expression_method_create(
            identifier_expression_create(unicode_view_from_c_str("method"), source_location_create(1, 4)),
            function_header_tree_create(
                NULL, 0,
                expression_allocate(expression_from_identifier(identifier_expression_create(
                                        unicode_view_from_c_str("return_type"), source_location_create(1, 14))),
                                    &pool)));
        test_successful_parse(expression_from_interface(interface_expression_create(
                                  generic_parameter_list_create(NULL, 0), source_location_create(0, 0), methods, 1)),
                              unicode_string_from_c_str("interface\n"
                                                        "    method(): return_type"),
                              false, &pool);
    }

    {
        interface_expression_method *const methods = allocate_array(2, sizeof(*methods));
        methods[0] = interface_expression_method_create(
            identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(1, 4)),
            function_header_tree_create(
                NULL, 0, expression_allocate(expression_from_identifier(identifier_expression_create(
                                                 unicode_view_from_c_str("c"), source_location_create(1, 9))),
                                             &pool)));
        methods[1] = interface_expression_method_create(
            identifier_expression_create(unicode_view_from_c_str("b"), source_location_create(2, 4)),
            function_header_tree_create(
                NULL, 0, expression_allocate(expression_from_identifier(identifier_expression_create(
                                                 unicode_view_from_c_str("d"), source_location_create(2, 9))),
                                             &pool)));
        test_successful_parse(expression_from_interface(interface_expression_create(
                                  generic_parameter_list_create(NULL, 0), source_location_create(0, 0), methods, 2)),
                              unicode_string_from_c_str("interface\n"
                                                        "    a(): c\n"
                                                        "    b(): d"),
                              false, &pool);
    }

    test_successful_parse(expression_from_struct(struct_expression_create(
                              generic_parameter_list_create(NULL, 0), source_location_create(0, 0), NULL, 0)),
                          unicode_string_from_c_str("struct"), false, &pool);

    {
        struct_expression_element *const elements = allocate_array(1, sizeof(*elements));
        elements[0] = struct_expression_element_create(
            identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(1, 4)),
            expression_from_identifier(
                identifier_expression_create(unicode_view_from_c_str("c"), source_location_create(1, 7))));
        test_successful_parse(expression_from_struct(struct_expression_create(
                                  generic_parameter_list_create(NULL, 0), source_location_create(0, 0), elements, 1)),
                              unicode_string_from_c_str("struct\n"
                                                        "    a: c"),
                              false, &pool);
    }

    {
        struct_expression_element *const elements = allocate_array(2, sizeof(*elements));
        elements[0] = struct_expression_element_create(
            identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(1, 4)),
            expression_from_identifier(
                identifier_expression_create(unicode_view_from_c_str("c"), source_location_create(1, 7))));
        elements[1] = struct_expression_element_create(
            identifier_expression_create(unicode_view_from_c_str("b"), source_location_create(2, 4)),
            expression_from_identifier(
                identifier_expression_create(unicode_view_from_c_str("d"), source_location_create(2, 7))));
        test_successful_parse(expression_from_struct(struct_expression_create(
                                  generic_parameter_list_create(NULL, 0), source_location_create(0, 0), elements, 2)),
                              unicode_string_from_c_str("struct\n"
                                                        "    a: c\n"
                                                        "    b: d"),
                              false, &pool);
    }

    {
        unicode_view *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = unicode_view_from_c_str("a");
        test_successful_parse(expression_from_enum(enum_expression_create(
                                  source_location_create(0, 0), generic_parameter_list_create(parameters, 1), NULL, 0)),
                              unicode_string_from_c_str("enum[a]"), false, &pool);
    }

    {
        unicode_view *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = unicode_view_from_c_str("a");
        parameters[1] = unicode_view_from_c_str("b");
        test_successful_parse(expression_from_enum(enum_expression_create(
                                  source_location_create(0, 0), generic_parameter_list_create(parameters, 2), NULL, 0)),
                              unicode_string_from_c_str("enum[a, b]"), false, &pool);
    }

    {
        expression *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_identifier(
            identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 2)));
        test_successful_parse(expression_from_generic_instantiation(generic_instantiation_expression_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("x"), source_location_create(0, 0))),
                                                      &pool),
                                  arguments, 1)),
                              unicode_string_from_c_str("x[a]"), false, &pool);
    }

    {
        expression *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_identifier(
            identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 2)));
        arguments[1] = expression_from_identifier(
            identifier_expression_create(unicode_view_from_c_str("b"), source_location_create(0, 5)));
        test_successful_parse(expression_from_generic_instantiation(generic_instantiation_expression_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("x"), source_location_create(0, 0))),
                                                      &pool),
                                  arguments, 2)),
                              unicode_string_from_c_str("x[a, b]"), false, &pool);
    }

    expression_pool_free(pool);
    test_function_calls();
    test_comment();
    test_loops();
    test_comparison();
    test_assignments();
    test_match_cases();
    test_lambdas();
    test_new_lines();
}

static void test_new_lines(void)
{
    expression_pool pool = expression_pool_create();
    // Empty line with just whitespace
    {
        unicode_string const input = unicode_string_from_c_str("  ");
        test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, NULL, 0};
        expression_parser parser = expression_parser_create(&user.base, handle_error, &user, &pool);

        sequence actual = parse_program(&parser);
        REQUIRE(actual.length == 0);

        expression_parser_free(parser);
        sequence_free(&actual);
        unicode_string_free(&input);
    }

    // Empty line with just indentation
    {
        unicode_string const input = unicode_string_from_c_str("    ");
        test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, NULL, 0};
        expression_parser parser = expression_parser_create(&user.base, handle_error, &user, &pool);

        sequence actual = parse_program(&parser);
        REQUIRE(actual.length == 0);

        expression_parser_free(parser);
        sequence_free(&actual);
        unicode_string_free(&input);
    }

    // One trailing whitespace at the end
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 7)));

        expression unequals_expression =
            expression_from_binary_operator(binary_operator_expression_create(left, right, not_equals));

        unicode_string const input = unicode_string_from_c_str("200 != 2 ");
        test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, NULL, 0};
        expression_parser parser = expression_parser_create(&user.base, handle_error, &user, &pool);

        sequence actual = parse_program(&parser);
        REQUIRE(actual.length == 1);
        REQUIRE(expression_equals(&actual.elements[0], &unequals_expression));

        expression_parser_free(parser);
        expression_free(unequals_expression);
        sequence_free(&actual);
        unicode_string_free(&input);
    }

    // An empty line within a loop
    {
        unicode_string const input = unicode_string_from_c_str("loop\n"
                                                               "    \n"
                                                               "    break");
        expression *elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_break(break_expression_create(source_location_create(2, 4), NULL));
        expression loop_expression = expression_from_loop(sequence_create(elements, 1, source_location_create(1, 0)));

        test_parser_user user = {{input.data, input.length, source_location_create(0, 0)}, NULL, 0};
        expression_parser parser = expression_parser_create(&user.base, handle_error, &user, &pool);

        sequence actual = parse_program(&parser);
        REQUIRE(actual.length == 1);
        REQUIRE(expression_equals(&actual.elements[0], &loop_expression));

        expression_parser_free(parser);
        expression_free(loop_expression);
        sequence_free(&actual);
        unicode_string_free(&input);
    }
    expression_pool_free(pool);
}

static void test_lambdas()
{
    expression_pool pool = expression_pool_create();

    // Lambda with direct return value
    test_successful_parse(expression_from_lambda(lambda_create(
                              generic_parameter_list_create(NULL, 0), function_header_tree_create(NULL, 0, NULL),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(0, 3))),
                                                  &pool),
                              source_location_create(0, 0))),
                          unicode_string_from_c_str("() 1"), false, &pool);

    // Lambda with return type and implicit return
    {
        expression *result_expression = expression_pool_allocate(&pool);
        *result_expression = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 4)));

        expression *const elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(1, 4)));
        test_successful_parse(
            expression_from_lambda(lambda_create(
                generic_parameter_list_create(NULL, 0), function_header_tree_create(NULL, 0, result_expression),
                expression_allocate(
                    expression_from_sequence(sequence_create(elements, 1, source_location_create(1, 0))), &pool),
                source_location_create(0, 0))),
            unicode_string_from_c_str("(): 1\n"
                                      "    1"),
            false, &pool);
    }

    // Lambda returning a lamda
    test_successful_parse(
        expression_from_lambda(
            lambda_create(generic_parameter_list_create(NULL, 0), function_header_tree_create(NULL, 0, NULL),
                          expression_allocate(
                              expression_from_lambda(lambda_create(
                                  generic_parameter_list_create(NULL, 0), function_header_tree_create(NULL, 0, NULL),
                                  expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                          integer_create(0, 1), source_location_create(0, 6))),
                                                      &pool),
                                  source_location_create(0, 3))),
                              &pool),
                          source_location_create(0, 0))),
        unicode_string_from_c_str("() () 1"), false, &pool);

    // Lambda without a return type
    {
        expression *const elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(1, 4)));
        test_successful_parse(
            expression_from_lambda(lambda_create(
                generic_parameter_list_create(NULL, 0), function_header_tree_create(NULL, 0, NULL),
                expression_allocate(
                    expression_from_sequence(sequence_create(elements, 1, source_location_create(1, 0))), &pool),
                source_location_create(0, 0))),
            unicode_string_from_c_str("()\n"
                                      "    1"),
            false, &pool);
    }
    // Lamda with a typed parameter and no return type
    {
        expression *const elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(1, 4)));
        parameter *const parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] =
            parameter_create(identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 1)),
                             expression_allocate(expression_from_identifier(identifier_expression_create(
                                                     unicode_view_from_c_str("type"), source_location_create(0, 4))),
                                                 &pool));
        test_successful_parse(
            expression_from_lambda(lambda_create(
                generic_parameter_list_create(NULL, 0), function_header_tree_create(parameters, 1, NULL),
                expression_allocate(
                    expression_from_sequence(sequence_create(elements, 1, source_location_create(1, 0))), &pool),
                source_location_create(0, 0))),
            unicode_string_from_c_str("(a: type)\n"
                                      "    1"),
            false, &pool);
    }

    // Lambda with two paramaters
    {
        expression *const elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(1, 4)));
        parameter *const parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] =
            parameter_create(identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 1)),
                             expression_allocate(expression_from_identifier(identifier_expression_create(
                                                     unicode_view_from_c_str("b"), source_location_create(0, 4))),
                                                 &pool));
        parameters[1] =
            parameter_create(identifier_expression_create(unicode_view_from_c_str("c"), source_location_create(0, 7)),
                             expression_allocate(expression_from_identifier(identifier_expression_create(
                                                     unicode_view_from_c_str("d"), source_location_create(0, 10))),
                                                 &pool));
        test_successful_parse(
            expression_from_lambda(lambda_create(
                generic_parameter_list_create(NULL, 0), function_header_tree_create(parameters, 2, NULL),
                expression_allocate(
                    expression_from_sequence(sequence_create(elements, 1, source_location_create(1, 0))), &pool),
                source_location_create(0, 0))),
            unicode_string_from_c_str("(a: b, c: d)\n"
                                      "    1"),
            false, &pool);
    }

    expression_pool_free(pool);
}

static void test_comparison(void)
{
    expression_pool pool = expression_pool_create();
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 7)));
        test_successful_parse(
            expression_from_binary_operator(binary_operator_expression_create(left, right, not_equals)),
            unicode_string_from_c_str("200 != 2"), true, &pool);
    }

    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 7)));
        test_successful_parse(
            expression_from_binary_operator(binary_operator_expression_create(left, right, less_than_or_equals)),
            unicode_string_from_c_str("200 <= 2"), true, &pool);
    }
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 7)));
        test_successful_parse(
            expression_from_binary_operator(binary_operator_expression_create(left, right, greater_than_or_equals)),
            unicode_string_from_c_str("200 >= 2"), true, &pool);
    }
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 7)));
        test_successful_parse(expression_from_binary_operator(binary_operator_expression_create(left, right, equals)),
                              unicode_string_from_c_str("200 == 2"), true, &pool);
    }
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 6)));
        test_successful_parse(
            expression_from_binary_operator(binary_operator_expression_create(left, right, less_than)),
            unicode_string_from_c_str("200 < 2"), true, &pool);
    }
    {
        expression *left = expression_pool_allocate(&pool);
        *left = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 200), source_location_create(0, 0)));

        expression *right = expression_pool_allocate(&pool);
        *right = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 6)));
        test_successful_parse(
            expression_from_binary_operator(binary_operator_expression_create(left, right, greater_than)),
            unicode_string_from_c_str("200 > 2"), true, &pool);
    }
    expression_pool_free(pool);
}

static void test_assignments(void)
{
    expression_pool pool = expression_pool_create();
    test_successful_parse(expression_from_binary_operator(binary_operator_expression_create(
                              expression_allocate(expression_from_identifier(identifier_expression_create(
                                                      unicode_view_from_c_str("a"), source_location_create(0, 0))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(0, 5))),
                                                  &pool),
                              equals)),
                          unicode_string_from_c_str("a == 1"), true, &pool);

    test_successful_parse(
        expression_from_declare(
            declare_create(identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 4)),
                           NULL, expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                         integer_create(0, 1), source_location_create(0, 8))),
                                                     &pool))),
        unicode_string_from_c_str("let a = 1"), true, &pool);

    test_successful_parse(expression_from_declare(declare_create(
                              identifier_expression_create(unicode_view_from_c_str("a"), source_location_create(0, 4)),
                              expression_allocate(expression_from_identifier(identifier_expression_create(
                                                      unicode_view_from_c_str("int"), source_location_create(0, 8))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(0, 14))),
                                                  &pool))),
                          unicode_string_from_c_str("let a : int = 1"), true, &pool);
    expression_pool_free(pool);
}

static void test_match_cases(void)
{
    expression_pool pool = expression_pool_create();
    {
        match_case *const cases = allocate_array(1, sizeof(*cases));
        cases[0] =
            match_case_create(expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(1, 9))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 2), source_location_create(1, 12))),
                                                  &pool));
        test_successful_parse(expression_from_match(match_create(
                                  source_location_create(0, 0),
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("a"), source_location_create(0, 6))),
                                                      &pool),
                                  cases, 1)),
                              unicode_string_from_c_str("match a\n"
                                                        "    case 1: 2\n"),
                              false, &pool);
    }

    {
        match_case *const cases = allocate_array(2, sizeof(*cases));
        cases[0] =
            match_case_create(expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 1), source_location_create(1, 9))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 2), source_location_create(1, 12))),
                                                  &pool));
        cases[1] =
            match_case_create(expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 3), source_location_create(2, 9))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 4), source_location_create(2, 12))),
                                                  &pool));
        test_successful_parse(expression_from_match(match_create(
                                  source_location_create(0, 0),
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("a"), source_location_create(0, 6))),
                                                      &pool),
                                  cases, 2)),
                              unicode_string_from_c_str("match a\n"
                                                        "    case 1: 2\n"
                                                        "    case 3: 4\n"),
                              false, &pool);
    }

    {
        match_case *const cases = allocate_array(2, sizeof(*cases));
        expression *case_0 = allocate_array(1, sizeof(*case_0));
        case_0[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(2, 8)));
        cases[0] = match_case_create(
            expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                    integer_create(0, 1), source_location_create(1, 9))),
                                &pool),
            expression_allocate(
                expression_from_sequence(sequence_create(case_0, 1, source_location_create(2, 0))), &pool));
        cases[1] =
            match_case_create(expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 3), source_location_create(3, 9))),
                                                  &pool),
                              expression_allocate(expression_from_integer_literal(integer_literal_expression_create(
                                                      integer_create(0, 4), source_location_create(3, 12))),
                                                  &pool));
        test_successful_parse(expression_from_match(match_create(
                                  source_location_create(0, 0),
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("a"), source_location_create(0, 6))),
                                                      &pool),
                                  cases, 2)),
                              unicode_string_from_c_str("match a\n"
                                                        "    case 1:\n"
                                                        "        2\n"
                                                        "    case 3: 4\n"),
                              false, &pool);
    }
    expression_pool_free(pool);
}

static void test_function_calls(void)
{
    expression_pool pool = expression_pool_create();
    {
        tuple const arguments = tuple_create(NULL, 0, source_location_create(0, 0));
        test_successful_parse(expression_from_call(call_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                                      &pool),
                                  arguments, source_location_create(0, 2))),
                              unicode_string_from_c_str("f()"), false, &pool);
    }
    {
        tuple const arguments = tuple_create(NULL, 0, source_location_create(0, 0));
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_call(call_create(
                        expression_allocate(expression_from_identifier(identifier_expression_create(
                                                unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                            &pool),
                        arguments, source_location_create(0, 2))),
                    &pool),
                arguments, source_location_create(0, 4))),
            unicode_string_from_c_str("f()()"), false, &pool);
    }
    {
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)));
        tuple const arguments_tuple = tuple_create(arguments, 1, source_location_create(0, 0));
        test_successful_parse(expression_from_call(call_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                                      &pool),
                                  arguments_tuple, source_location_create(0, 3))),
                              unicode_string_from_c_str("f(1)"), false, &pool);
    }
    {
        expression *arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 1), source_location_create(0, 2)));
        arguments[1] = expression_from_integer_literal(
            integer_literal_expression_create(integer_create(0, 2), source_location_create(0, 5)));
        tuple const arguments_tuple = tuple_create(arguments, 2, source_location_create(0, 0));
        test_successful_parse(expression_from_call(call_create(
                                  expression_allocate(expression_from_identifier(identifier_expression_create(
                                                          unicode_view_from_c_str("f"), source_location_create(0, 0))),
                                                      &pool),
                                  arguments_tuple, source_location_create(0, 6))),
                              unicode_string_from_c_str("f(1, 2)"), false, &pool);
    }
    expression_pool_free(pool);
}

static void test_comment(void)
{
    expression_pool pool = expression_pool_create();
    {
        expression const expr = expression_from_comment(
            comment_expression_create(unicode_string_from_c_str("Testing the comments"), source_location_create(0, 0)));
        test_successful_parse(expr, unicode_string_from_c_str("//Testing the comments"), false, &pool);
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expression_from_comment(
            comment_expression_create(unicode_string_from_c_str("Testing the comments"), source_location_create(1, 4)));
        elements[1] = expression_from_break(break_expression_create(source_location_create(2, 4), NULL));
        test_successful_parse(expression_from_loop(sequence_create(elements, 2, source_location_create(1, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    //Testing the comments\n"
                                                        "    break"),
                              false, &pool);
    }
    expression_pool_free(pool);
}

static void test_loops(void)
{
    expression_pool pool = expression_pool_create();
    {
        expression *elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_break(break_expression_create(source_location_create(1, 4), NULL));
        test_successful_parse(expression_from_loop(sequence_create(elements, 1, source_location_create(1, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    break"),
                              false, &pool);
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expression_from_identifier(
            identifier_expression_create(unicode_view_from_c_str("f"), source_location_create(1, 4)));
        elements[1] = expression_from_break(break_expression_create(source_location_create(2, 4), NULL));
        test_successful_parse(expression_from_loop(sequence_create(elements, 2, source_location_create(1, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    f\n"
                                                        "    break"),
                              false, &pool);
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] = expression_from_identifier(
            identifier_expression_create(unicode_view_from_c_str("f"), source_location_create(1, 4)));
        elements[1] = expression_from_break(break_expression_create(source_location_create(2, 4), NULL));
        test_successful_parse(expression_from_loop(sequence_create(elements, 2, source_location_create(1, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    f\n"
                                                        "    break\n"),
                              false, &pool);
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(break_expression_create(source_location_create(2, 8), NULL));
        expression *outer_loop = allocate_array(1, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1, source_location_create(1, 0)));
        test_successful_parse(expression_from_loop(sequence_create(outer_loop, 1, source_location_create(2, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    loop\n"
                                                        "        break\n"),
                              false, &pool);
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(break_expression_create(source_location_create(2, 8), NULL));
        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1, source_location_create(1, 0)));
        outer_loop[1] = expression_from_break(break_expression_create(source_location_create(3, 4), NULL));
        test_successful_parse(expression_from_loop(sequence_create(outer_loop, 2, source_location_create(2, 0))),
                              unicode_string_from_c_str("loop\n"
                                                        "    loop\n"
                                                        "        break\n"
                                                        "    break\n"),
                              false, &pool);
    }

    expression_pool_free(pool);
}
