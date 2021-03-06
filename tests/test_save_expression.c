#include "test_save_expression.h"
#include "lpg_allocate.h"
#include "lpg_expression_pool.h"
#include "lpg_save_expression.h"
#include "lpg_stream_writer.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

static void testing_comment_expressions(void);
static void test_integer_saving(void);
static expression expression_from_integer(const integer number);
static identifier_expression identifier_expression_from_c_string(const char *c_string)
{
    return identifier_expression_create(unicode_view_from_c_str(c_string), source_location_create(0, 0));
}

static void check_expression_rendering(expression tree, char const *expected)
{
    memory_writer buffer = {NULL, 0, 0};
    whitespace_state whitespace = {0, 0};
    REQUIRE(save_expression(memory_writer_erase(&buffer), &tree, whitespace) == success_yes);
    if (!memory_writer_equals(buffer, expected))
    {
        printf("Expected  (%zu): [%s]\n", strlen(expected), expected);
        printf("Generated (%zu): [", buffer.used);
        fwrite(buffer.data, 1, buffer.used, stdout);
        printf("]\n");
        FAIL();
    }
    memory_writer_free(&buffer);
    expression_free(tree);
}

void test_save_expression(void)
{
    check_expression_rendering(
        expression_from_string(string_expression_create(unicode_view_from_c_str("\"\""), source_location_create(0, 0))),
        "\"\"");
    check_expression_rendering(expression_from_string(string_expression_create(
                                   unicode_view_from_c_str("\"abc\""), source_location_create(0, 0))),
                               "\"abc\"");
    check_expression_rendering(expression_from_string(string_expression_create(
                                   unicode_view_from_c_str("\"\"\'\\\""), source_location_create(0, 0))),
                               "\"\\\"\\\'\\\\\"");
    test_integer_saving();

    expression_pool pool = expression_pool_create();
    {
        expression *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_string(
            string_expression_create(unicode_view_from_c_str("\"test\""), source_location_create(0, 0)));
        arguments[1] = expression_from_identifier(identifier_expression_from_c_string("a"));
        check_expression_rendering(
            expression_from_call(call_create(
                expression_allocate(expression_from_identifier(identifier_expression_from_c_string("f")), &pool),
                tuple_create(arguments, 2, source_location_create(0, 0)), source_location_create(0, 0))),
            "f(\"test\", a)");
    }
    {
        parameter *parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = parameter_create(
            identifier_expression_from_c_string("a"),
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("uint32")), &pool));
        check_expression_rendering(
            expression_from_lambda(
                lambda_create(generic_parameter_list_create(NULL, 0), function_header_tree_create(parameters, 1, NULL),
                              expression_allocate(expression_from_integer(integer_create(0, 1234)), &pool),
                              source_location_create(0, 0))),
            "(a: uint32) 1234");
    }

    {
        expression *arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer(integer_create(0, 0));
        arguments[1] = expression_from_integer(integer_create(0, 10));
        check_expression_rendering(
            expression_from_declare(declare_create(
                identifier_expression_from_c_string("a"),
                expression_allocate(
                    expression_from_call(call_create(
                        expression_allocate(
                            expression_from_identifier(identifier_expression_from_c_string("integer")), &pool),
                        tuple_create(arguments, 2, source_location_create(0, 0)), source_location_create(0, 0))),
                    &pool),
                expression_allocate(expression_from_integer(integer_create(0, 6)), &pool))),
            "let a : integer(0, 10) = 6");
    }

    check_expression_rendering(expression_from_declare(declare_create(
                                   identifier_expression_from_c_string("a"), NULL,
                                   expression_allocate(expression_from_integer(integer_create(0, 6)), &pool))),
                               "let a = 6");

    check_expression_rendering(
        expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals)),
        "a == 123");

    check_expression_rendering(
        expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(
                expression_from_access_structure(access_structure_create(
                    expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
                    identifier_expression_from_c_string("m"))),
                &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals)),
        "a.m == 123");

    check_expression_rendering(expression_from_return(expression_allocate(
                                   expression_from_identifier(identifier_expression_from_c_string("a")), &pool)),
                               "return a");

    {
        expression *loop_body = allocate_array(1, sizeof(*loop_body));
        loop_body[0] = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals));
        check_expression_rendering(expression_from_loop(sequence_create(loop_body, 1, source_location_create(1, 0))),
                                   "loop\n"
                                   "    a == 123");
    }

    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals));
        expression *outer_loop = allocate_array(1, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1, source_location_create(0, 0)));
        check_expression_rendering(expression_from_loop(sequence_create(outer_loop, 1, source_location_create(0, 0))),
                                   "loop\n"
                                   "    loop\n"
                                   "        a == 123");
    }

    {
        expression *body = allocate_array(2, sizeof(*body));
        body[0] = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals));
        body[1] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        check_expression_rendering(expression_from_loop(sequence_create(body, 2, source_location_create(0, 0))),
                                   "loop\n"
                                   "    a == 123\n"
                                   "    break");
    }
    {
        expression *inner_loop = allocate_array(2, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals));
        outer_loop[1] = expression_from_loop(sequence_create(inner_loop, 1, source_location_create(0, 0)));
        check_expression_rendering(expression_from_loop(sequence_create(outer_loop, 2, source_location_create(0, 0))),
                                   "loop\n"
                                   "    a == 123\n"
                                   "    loop\n"
                                   "        break");
    }

    check_expression_rendering(
        expression_from_break(break_expression_create(source_location_create(0, 0), NULL)), "break");

    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));

        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_binary_operator(binary_operator_expression_create(
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("a")), &pool),
            expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), equals));
        outer_loop[1] = expression_from_loop(sequence_create(inner_loop, 1, source_location_create(0, 0)));

        parameter *parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = parameter_create(
            identifier_expression_from_c_string("a"),
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("uint32")), &pool));

        check_expression_rendering(
            expression_from_lambda(lambda_create(
                generic_parameter_list_create(NULL, 0), function_header_tree_create(parameters, 1, NULL),
                expression_allocate(
                    expression_from_loop(sequence_create(outer_loop, 2, source_location_create(0, 0))), &pool),
                source_location_create(0, 0))),
            "(a: uint32) loop\n"
            "    a == 123\n"
            "    loop\n"
            "        break");
    }

    {
        parameter *parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = parameter_create(
            identifier_expression_from_c_string("a"),
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("float")), &pool));
        parameters[1] = parameter_create(
            identifier_expression_from_c_string("b"),
            expression_allocate(expression_from_identifier(identifier_expression_from_c_string("string")), &pool));

        check_expression_rendering(
            expression_from_lambda(
                lambda_create(generic_parameter_list_create(NULL, 0), function_header_tree_create(parameters, 2, NULL),
                              expression_allocate(expression_from_integer(integer_create(0, 123)), &pool),
                              source_location_create(0, 0))),
            "(a: float, b: string) 123");
    }

    {
        match_case *cases = allocate_array(1, sizeof(*cases));
        cases[0] = match_case_create(expression_allocate(expression_from_integer(integer_create(0, 123)), &pool),
                                     expression_allocate(expression_from_integer(integer_create(0, 456)), &pool));
        check_expression_rendering(
            expression_from_match(
                match_create(source_location_create(0, 0),
                             expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), cases, 1)),
            "match 123\n"
            "    case 123: 456\n");
    }

    {
        match_case *cases = allocate_array(2, sizeof(*cases));
        cases[0] = match_case_create(expression_allocate(expression_from_integer(integer_create(0, 123)), &pool),
                                     expression_allocate(expression_from_integer(integer_create(0, 456)), &pool));
        cases[1] = match_case_create(expression_allocate(expression_from_integer(integer_create(0, 124)), &pool),
                                     expression_allocate(expression_from_integer(integer_create(0, 457)), &pool));
        check_expression_rendering(
            expression_from_match(
                match_create(source_location_create(0, 0),
                             expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), cases, 2)),
            "match 123\n"
            "    case 123: 456\n"
            "    case 124: 457\n");
    }

    {
        match_case *cases = allocate_array(2, sizeof(*cases));
        cases[0] = match_case_create(expression_allocate(expression_from_integer(integer_create(0, 123)), &pool),
                                     expression_allocate(expression_from_integer(integer_create(0, 456)), &pool));
        expression *const case_1 = allocate_array(2, sizeof(*case_1));
        case_1[0] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        case_1[1] = expression_from_break(break_expression_create(source_location_create(0, 0), NULL));
        cases[1] = match_case_create(
            expression_allocate(expression_from_integer(integer_create(0, 124)), &pool),
            expression_allocate(
                expression_from_sequence(sequence_create(case_1, 2, source_location_create(0, 0))), &pool));

        check_expression_rendering(
            expression_from_match(
                match_create(source_location_create(0, 0),
                             expression_allocate(expression_from_integer(integer_create(0, 123)), &pool), cases, 2)),
            "match 123\n"
            "    case 123: 456\n"
            "    case 124:\n"
            "        break\n"
            "        break\n");
    }

    expression_pool_free(pool);
    testing_comment_expressions();
}

static void testing_comment_expressions(void)
{
    {
        unicode_string const content = unicode_string_from_c_str("Test");
        expression const comment =
            expression_from_comment(comment_expression_create(content, source_location_create(0, 0)));
        check_expression_rendering(comment, "//Test");
    }

    {
        unicode_string const content = unicode_string_from_c_str("Test\nTesting");
        expression const comment =
            expression_from_comment(comment_expression_create(content, source_location_create(0, 0)));
        check_expression_rendering(comment, "/*Test\nTesting*/");
    }
}

static expression expression_from_integer(const integer number)
{
    return expression_from_integer_literal(integer_literal_expression_create(number, source_location_create(0, 0)));
}

static void test_integer_saving(void)
{
    check_expression_rendering(expression_from_integer(integer_create(0, 0)), "0");
    check_expression_rendering(expression_from_integer(integer_create(0, 1)), "1");
    check_expression_rendering(expression_from_integer(integer_create(0, 9)), "9");
    check_expression_rendering(expression_from_integer(integer_create(0, 10)), "10");
    check_expression_rendering(expression_from_integer(integer_create(0, 11)), "11");
    check_expression_rendering(expression_from_integer(integer_create(0, 1000)), "1000");
    check_expression_rendering(expression_from_integer(integer_create(1, 0)), "18446744073709551616");

    check_expression_rendering(expression_from_integer(integer_create(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu)),
                               "340282366920938463463374607431768211455");
}
