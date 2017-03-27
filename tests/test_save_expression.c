#include "test_save_expression.h"
#include "lpg_expression.h"
#include "test.h"
#include "lpg_allocate.h"
#include <string.h>
#include <stdlib.h>
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_stream_writer.h"
#include "lpg_save_expression.h"

static void check_expression_rendering(expression tree, char const *expected)
{
    memory_writer buffer = {NULL, 0, 0};
    REQUIRE(save_expression(memory_writer_erase(&buffer), &tree, 0) == success);
    REQUIRE(memory_writer_equals(buffer, expected));
    memory_writer_free(&buffer);
    expression_free(&tree);
}

void test_save_expression(void)
{
    check_expression_rendering(expression_from_builtin(builtin_unit), "()");
    check_expression_rendering(
        expression_from_builtin(builtin_empty_structure), "{}");
    check_expression_rendering(
        expression_from_builtin(builtin_empty_variant), "{}");
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
                arguments, 2)),
            "f(\"test\", a)");
    }
    {
        check_expression_rendering(
            expression_from_lambda(lambda_create(
                expression_allocate(expression_from_identifier(
                    unicode_string_from_c_str("uint32"))),
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("a"))),
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 1234))))),
            "(a: uint32) => 1234");
    }
    check_expression_rendering(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123))))),
        "a = 123\n");

    check_expression_rendering(
        expression_from_return(expression_allocate(
            expression_from_identifier(unicode_string_from_c_str("a")))),
        "return a\n");

    check_expression_rendering(
        expression_from_loop(expression_allocate(expression_from_assign(
            assign_create(expression_allocate(expression_from_identifier(
                              unicode_string_from_c_str("a"))),
                          expression_allocate(expression_from_integer_literal(
                              integer_create(0, 123))))))),
        "loop\n"
        "    a = 123\n");

    check_expression_rendering(
        expression_from_loop(expression_allocate(expression_from_loop(
            expression_allocate(expression_from_assign(assign_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("a"))),
                expression_allocate(expression_from_integer_literal(
                    integer_create(0, 123))))))))),
        "loop\n"
        "    loop\n"
        "        a = 123\n");

    {
        expression *body = allocate_array(2, sizeof(*body));
        body[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        body[1] = expression_from_break();
        check_expression_rendering(
            expression_from_loop(expression_allocate(
                expression_from_sequence(sequence_create(body, 2)))),
            "loop\n"
            "    a = 123\n"
            "    break\n");
    }
    {
        expression *body = allocate_array(2, sizeof(*body));
        body[0] = expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        body[1] =
            expression_from_loop(expression_allocate(expression_from_break()));
        check_expression_rendering(
            expression_from_loop(expression_allocate(
                expression_from_sequence(sequence_create(body, 2)))),
            "loop\n"
            "    a = 123\n"
            "    loop\n"
            "        break\n");
    }
    check_expression_rendering(expression_from_break(), "break\n");
}
