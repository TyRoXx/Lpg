#include "test_save_expression.h"
#include "lpg_expression.h"
#include "test.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_stream_writer.h"
#include "lpg_save_expression.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void check_expression_rendering(expression tree, char const *expected)
{
    memory_writer buffer = {NULL, 0, 0};
    whitespace_state whitespace = {0, 0};
    REQUIRE(save_expression(memory_writer_erase(&buffer), &tree, whitespace) ==
            success);
    if (!memory_writer_equals(buffer, expected))
    {
        printf("Expected  (%zu): [%s]\n", strlen(expected), expected);
        printf("Generated (%zu): [", buffer.used);
        fwrite(buffer.data, 1, buffer.used, stdout);
        printf("]\n");
        FAIL();
    }
    memory_writer_free(&buffer);
    expression_free(&tree);
}

void test_save_expression(void)
{
    check_expression_rendering(
        expression_from_unicode_string(unicode_string_from_c_str("")), "\"\"");
    check_expression_rendering(
        expression_from_unicode_string(unicode_string_from_c_str("abc")),
        "\"abc\"");
    check_expression_rendering(
        expression_from_unicode_string(unicode_string_from_c_str("\"\'\\")),
        "\"\\\"\\\'\\\\\"");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 0)), "0");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 1)), "1");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 9)), "9");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 10)), "10");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 11)), "11");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(0, 1000)), "1000");
    check_expression_rendering(
        expression_from_integer_literal(integer_create(1, 0)),
        "18446744073709551616");
    check_expression_rendering(expression_from_integer_literal(integer_create(
                                   0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu)),
                               "340282366920938463463374607431768211455");
    {
        expression *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] =
            expression_from_unicode_string(unicode_string_from_c_str("test"));
        arguments[1] =
            expression_from_identifier(unicode_string_from_c_str("a"));
        check_expression_rendering(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("f"))),
                tuple_create(arguments, 2))),
            "f(\"test\", a)");
    }
    {
        parameter *parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = parameter_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(expression_from_identifier(
                unicode_string_from_c_str("uint32"))));
        check_expression_rendering(
            expression_from_lambda(lambda_create(
                parameters, 1,
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 1234))))),
            "(a: uint32) => 1234");
    }

    {
        expression *arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 0));
        arguments[1] = expression_from_integer_literal(integer_create(0, 10));
        check_expression_rendering(
            expression_from_declare(declare_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("a"))),
                expression_allocate(expression_from_call(
                    call_create(expression_allocate(expression_from_identifier(
                                    unicode_string_from_c_str("integer"))),
                                tuple_create(arguments, 2)))),
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 6))))),
            "a: integer(0, 10) = 6");
    }

    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))))),
        "a = 123");

    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(expression_from_make_identifier(
                expression_allocate(expression_from_identifier(
                    unicode_string_from_c_str("a"))))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))))),
        "*a = 123");

    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(expression_from_make_identifier(
                expression_allocate(expression_from_make_identifier(
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("b"))))))))),
        "a = **b");

    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_access_structure(access_structure_create(
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("a"))),
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("m")))))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))))),
        "a.m = 123");

    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_access_structure(access_structure_create(
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("a"))),
                    expression_allocate(expression_from_make_identifier(
                        expression_allocate(expression_from_identifier(
                            unicode_string_from_c_str("m")))))))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))))),
        "a.*m = 123");

    check_expression_rendering(
        expression_from_return(expression_allocate(
            expression_from_identifier(unicode_string_from_c_str("a")))),
        "return a");

    {
        expression *loop_body = allocate_array(1, sizeof(*loop_body));
        loop_body[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        check_expression_rendering(
            expression_from_loop(sequence_create(loop_body, 1)), "loop\n"
                                                                 "    a = 123");
    }

    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        expression *outer_loop = allocate_array(1, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1));
        check_expression_rendering(
            expression_from_loop(sequence_create(outer_loop, 1)),
            "loop\n"
            "    loop\n"
            "        a = 123");
    }

    {
        expression *body = allocate_array(2, sizeof(*body));
        body[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        body[1] = expression_from_break();
        check_expression_rendering(
            expression_from_loop(sequence_create(body, 2)), "loop\n"
                                                            "    a = 123\n"
                                                            "    break");
    }
    {
        expression *inner_loop = allocate_array(2, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break();
        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        outer_loop[1] = expression_from_loop(sequence_create(inner_loop, 1));
        check_expression_rendering(
            expression_from_loop(sequence_create(outer_loop, 2)),
            "loop\n"
            "    a = 123\n"
            "    loop\n"
            "        break");
    }

    check_expression_rendering(expression_from_break(), "break");

    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break();

        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        outer_loop[1] = expression_from_loop(sequence_create(inner_loop, 1));

        parameter *parameters = allocate_array(1, sizeof(*parameters));
        parameters[0] = parameter_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(expression_from_identifier(
                unicode_string_from_c_str("uint32"))));

        check_expression_rendering(
            expression_from_lambda(lambda_create(
                parameters, 1, expression_allocate(expression_from_loop(
                                   sequence_create(outer_loop, 2))))),
            "(a: uint32) => loop\n"
            "    a = 123\n"
            "    loop\n"
            "        break");
    }

    {
        parameter *parameters = allocate_array(2, sizeof(*parameters));
        parameters[0] = parameter_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(expression_from_identifier(
                unicode_string_from_c_str("float"))));
        parameters[1] = parameter_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("b"))),
            expression_allocate(expression_from_identifier(
                unicode_string_from_c_str("string"))));

        check_expression_rendering(
            expression_from_lambda(lambda_create(
                parameters, 2,
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 123))))),
            "(a: float, b: string) => 123");
    }

    check_expression_rendering(
        expression_from_tuple(tuple_create(NULL, 0)), "()");

    {
        expression *elements = allocate_array(1, sizeof(*elements));
        elements[0] =
            expression_from_identifier(unicode_string_from_c_str("a"));
        check_expression_rendering(
            expression_from_tuple(tuple_create(elements, 1)), "(a)");
    }

    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] =
            expression_from_identifier(unicode_string_from_c_str("a"));
        elements[1] = expression_from_integer_literal(integer_create(0, 123));
        check_expression_rendering(
            expression_from_tuple(tuple_create(elements, 2)), "(a, 123)");
    }

    {
        match_case *cases = allocate_array(1, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 456))));
        check_expression_rendering(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 123))),
                cases, 1)),
            "match 123\n"
            "    case 123: 456\n");
    }

    {
        match_case *cases = allocate_array(2, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 456))));
        cases[1] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 124))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 457))));
        check_expression_rendering(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 123))),
                cases, 2)),
            "match 123\n"
            "    case 123: 456\n"
            "    case 124: 457\n");
    }

    {
        match_case *cases = allocate_array(2, sizeof(*cases));
        cases[0] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 456))));
        expression *const sequence = allocate_array(2, sizeof(*sequence));
        sequence[0] = expression_from_break();
        sequence[1] = expression_from_break();
        cases[1] = match_case_create(
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 124))),
            expression_allocate(
                expression_from_sequence(sequence_create(sequence, 2))));
        check_expression_rendering(
            expression_from_match(match_create(
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 123))),
                cases, 2)),
            "match 123\n"
            "    case 123: 456\n"
            "    case 124:\n"
            "        break\n"
            "        break\n");
    }
}
