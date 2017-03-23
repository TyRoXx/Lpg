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

static success_indicator save_expression(stream_writer const to,
                                         expression const *value)
{
    switch (value->type)
    {
    case expression_type_lambda:
        UNREACHABLE();

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
    case expression_type_local:
    case expression_type_integer_literal:
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
        switch (stream_writer_write_string(to, "\""))
        {
        case failure:
            return failure;

        case success:
            break;
        }
        LPG_FOR(size_t, i, value->string.length)
        {
            switch (value->string.data[i])
            {
            case '\"':
            case '\'':
            case '\\':
                switch (stream_writer_write_string(to, "\\"))
                {
                case failure:
                    return failure;

                case success:
                    break;
                }
                break;
            }
            switch (stream_writer_write_utf8(to, value->string.data[i]))
            {
            case failure:
                return failure;

            case success:
                break;
            }
        }
        switch (stream_writer_write_string(to, "\""))
        {
        case failure:
            return failure;

        case success:
            break;
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
}
