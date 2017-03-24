#include "save_expression.h"
#include "lpg_expression.h"
#include "test.h"
#include "lpg_allocate.h"
#include <string.h>
#include <stdlib.h>
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_stream_writer.h"

static success_indicator save_expression(stream_writer const to,
                                         expression const *value)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_TRY(save_expression(to, value->lambda.parameter_name));
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, value->lambda.parameter_type));
        LPG_TRY(stream_writer_write_string(to, ") => "));
        LPG_TRY(save_expression(to, value->lambda.result));
        return success;

    case expression_type_builtin:
        switch (value->builtin)
        {
        case builtin_unit:
            return stream_writer_write_string(to, "()");

        case builtin_empty_structure:
            return stream_writer_write_string(to, "{}");

        case builtin_empty_variant:
            return stream_writer_write_string(to, "{}");
        }
        UNREACHABLE();

    case expression_type_call:
        LPG_TRY(save_expression(to, value->call.callee));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->call.number_of_arguments)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            LPG_TRY(save_expression(to, &value->call.arguments[i]));
        }
        LPG_TRY(stream_writer_write_string(to, ")"));
        return success;

    case expression_type_local:
        UNREACHABLE();

    case expression_type_integer_literal:
    {
        char formatted[39];
        size_t next_digit = sizeof(formatted) - 1;
        integer rest = value->integer_literal;
        for (;;)
        {
            integer_division const divided =
                integer_divide(rest, integer_create(0, 10));
            formatted[next_digit] = (char)((divided.remainder.low % 10u) + '0');
            rest = divided.quotient;
            if (rest.high == 0 && rest.low == 0)
            {
                break;
            }
            --next_digit;
        }
        return stream_writer_write_bytes(
            to, formatted + next_digit, sizeof(formatted) - next_digit);
    }

    case expression_type_integer_range:
    case expression_type_function:
    case expression_type_add_member:
    case expression_type_fill_structure:
    case expression_type_access_structure:
    case expression_type_add_to_variant:
    case expression_type_match:
    case expression_type_sequence:
    case expression_type_assignment:
        UNREACHABLE();

    case expression_type_string:
        LPG_TRY(stream_writer_write_string(to, "\""));
        LPG_FOR(size_t, i, value->string.length)
        {
            switch (value->string.data[i])
            {
            case '\"':
            case '\'':
            case '\\':
                LPG_TRY(stream_writer_write_string(to, "\\"));
                break;
            }
            LPG_TRY(stream_writer_write_utf8(to, value->string.data[i]));
        }
        LPG_TRY(stream_writer_write_string(to, "\""));
        return success;

    case expression_type_identifier:
        LPG_FOR(size_t, i, value->identifier.length)
        {
            LPG_TRY(stream_writer_write_utf8(to, value->string.data[i]));
        }
        return success;
    }
    UNREACHABLE();
}

static void check_expression_rendering(expression tree, char const *expected)
{
    memory_writer buffer = {NULL, 0, 0};
    REQUIRE(save_expression(memory_writer_erase(&buffer), &tree) == success);
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
}
