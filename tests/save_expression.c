#include "save_expression.h"
#include "lpg_expression.h"
#include "test.h"
#include "lpg_allocate.h"
#include <string.h>
#include <stdlib.h>
#include "lpg_assert.h"
#include "lpg_for.h"

typedef enum success_indicator
{
    success,
    failure
} success_indicator;

typedef struct stream_writer
{
    success_indicator (*write)(void *user, char const *data, size_t length);
    void *user;
} stream_writer;

static success_indicator stream_writer_write_string(stream_writer writer,
                                                    char const *c_str)
{
    return writer.write(writer.user, c_str, strlen(c_str));
}

static success_indicator
stream_writer_write_bytes(stream_writer writer, char const *data, size_t size)
{
    return writer.write(writer.user, data, size);
}

static success_indicator stream_writer_write_utf8(stream_writer writer,
                                                  unicode_code_point code_point)
{
    char code_units[4];
    code_units[0] = (char)code_point;
    /*TODO: encode UTF-8 properly*/
    return writer.write(writer.user, code_units, 1);
}

typedef struct memory_writer
{
    char *data;
    size_t reserved;
    size_t used;
} memory_writer;

static void memory_writer_free(memory_writer *writer)
{
    deallocate(writer->data);
}

static success_indicator memory_writer_write(void *user, char const *data,
                                             size_t length)
{
    memory_writer *writer = user;
    size_t new_used = (writer->used + length);
    if (new_used > writer->reserved)
    {
        size_t new_reserved = (writer->used * 2);
        if (new_reserved < new_used)
        {
            new_reserved = new_used;
        }
        writer->data = reallocate(writer->data, new_reserved);
        writer->reserved = new_reserved;
    }
    memmove(writer->data + writer->used, data, length);
    writer->used = new_used;
    return success;
}

static int memory_writer_equals(memory_writer const writer, char const *c_str)
{
    size_t length = strlen(c_str);
    if (length != writer.used)
    {
        return 0;
    }
    return !memcmp(c_str, writer.data, length);
}

static stream_writer memory_writer_erase(memory_writer *writer)
{
    stream_writer result = {memory_writer_write, writer};
    return result;
}

#define LPG_TRY(expression)                                                    \
    \
switch(expression)                                                             \
    \
{                                                                       \
        \
case failure : return failure;                                                 \
        \
case success : break;                                                          \
    \
}

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
        char formatted[100];
        size_t next_digit = sizeof(formatted) - 1;
        integer rest = value->integer_literal;
        for (;;)
        {
            integer_division const divided =
                integer_divide(rest, integer_create(0, 10));
            formatted[next_digit] = ((divided.remainder.low % 10u) + '0');
            --next_digit;
            rest = divided.quotient;
            if (rest.high == 0 && rest.low == 0)
            {
                break;
            }
        }
        return stream_writer_write_bytes(
            to, formatted + next_digit + 1, sizeof(formatted) - next_digit - 1);
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
