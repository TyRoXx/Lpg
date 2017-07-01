#include "test_c_backend.h"
#include "test.h"
#include "lpg_check.h"
#include "handle_parse_error.h"
#include <string.h>
#include "standard_library.h"
#include "lpg_c_backend.h"
#include "lpg_read_file.h"
#include "lpg_allocate.h"

static sequence parse(unicode_view const input)
{
    test_parser_user user = {
        {input.begin, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static unicode_view path_remove_leaf(unicode_view const full)
{
    char const *i = (full.begin + full.length);
    while ((i != full.begin) && (*i != '/') && (*i != '\\'))
    {
        --i;
    }
    return unicode_view_create(full.begin, (size_t)(i - full.begin));
}

static unicode_string path_combine(unicode_view const *begin, size_t count)
{
    size_t total_length = 0;
    for (size_t i = 0; i < count; ++i)
    {
        total_length += begin[i].length;
        total_length += 1;
    }
    unicode_string result = {allocate(total_length), total_length};
    size_t next_write = 0;
    for (size_t i = 0; i < count; ++i)
    {
        unicode_view const piece = begin[i];
        memcpy(result.data + next_write, piece.begin, piece.length);
        next_write += piece.length;
        result.data[next_write] = '/';
        ++next_write;
    }
    if (result.length)
    {
        result.data[result.length - 1] = '\0';
    }
    return result;
}

static void fix_line_endings(unicode_string *s)
{
    size_t length = s->length;
    size_t last_written = 0;
    for (size_t i = 0; i < length; ++i)
    {
        if (s->data[i] == '\r')
        {
            continue;
        }
        s->data[last_written] = s->data[i];
        ++last_written;
    }
    s->length = last_written;
}

static void check_generated_c_code(char const *const source,
                                   structure const non_empty_global,
                                   char const *const expected_boilerplate,
                                   char const *const expected_c_file_name)
{
    sequence root = parse(unicode_view_from_c_str(source));
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    memory_writer generated = {NULL, 0, 0};
    REQUIRE(success == generate_c(checked, memory_writer_erase(&generated)));
    unicode_view const pieces[] = {
        path_remove_leaf(unicode_view_from_c_str(__FILE__)),
        unicode_view_from_c_str("c_backend"),
        unicode_view_from_c_str(expected_c_file_name)};
    unicode_string const full_expected_file_path =
        path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    blob_or_error expected_or_error = read_file(full_expected_file_path.data);
    REQUIRE(!expected_or_error.error);
    unicode_string expected =
        unicode_string_validate(expected_or_error.success);
    REQUIRE(expected.length == expected_or_error.success.length);
    fix_line_endings(&expected);
    {
        size_t const expected_total_length =
            (strlen(expected_boilerplate) + expected.length);
        REQUIRE(generated.used == expected_total_length);
    }
    REQUIRE(
        unicode_view_equals(unicode_view_cut(memory_writer_content(generated),
                                             0, strlen(expected_boilerplate)),
                            unicode_view_from_c_str(expected_boilerplate)));
    REQUIRE(unicode_view_equals(
        unicode_view_cut(memory_writer_content(generated),
                         strlen(expected_boilerplate), generated.used),
        unicode_view_from_string(expected)));
    unicode_string_free(&full_expected_file_path);
    checked_program_free(&checked);
    memory_writer_free(&generated);
    unicode_string_free(&expected);
}

void test_c_backend(void)
{
    standard_library_description const std_library =
        describe_standard_library();

    check_generated_c_code("", std_library.globals, "", "0_empty.c");

    check_generated_c_code("print(\"Hello, world!\")\n", std_library.globals,
                           LPG_C_STDIO, "1_hello_world.c");

    check_generated_c_code("print(\"Hello, \")\nprint(\"world!\\n\")\n",
                           std_library.globals, LPG_C_STDIO, "2_print_twice.c");

    check_generated_c_code("loop\n"
                           "    print(\"Hello, world!\")\n"
                           "    break\n",
                           std_library.globals, LPG_C_STDIO, "3_loop_break.c");

    check_generated_c_code("let s = concat(\"123\", \"456\")\n"
                           "print(s)\n",
                           std_library.globals, LPG_C_STDIO,
                           "4_concat_compile_time.c");

    check_generated_c_code("print(read())\n", std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_STRING
                               LPG_C_STRING_REF LPG_C_STDIO LPG_C_READ,
                           "5_read.c");

    check_generated_c_code("loop\n"
                           "    print(read())\n",
                           std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_STRING
                               LPG_C_STRING_REF LPG_C_STDIO LPG_C_READ,
                           "6_loop_read.c");

    check_generated_c_code("assert(boolean.false)\n", std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_ASSERT,
                           "7_assert_false.c");

    check_generated_c_code("assert(boolean.true)\n", std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_ASSERT,
                           "8_assert_true.c");

    check_generated_c_code("assert(string-equals(read(), \"\"))\n",
                           std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_ASSERT LPG_C_STRING
                               LPG_C_STRING_REF LPG_C_STDIO LPG_C_READ,
                           "9_string_equals.c");

    check_generated_c_code("print(concat(\"a\", read()))\n",
                           std_library.globals,
                           LPG_C_STDLIB LPG_C_STDBOOL LPG_C_STRING
                               LPG_C_STRING_REF LPG_C_STDIO LPG_C_READ,
                           "10_concat.c");

    standard_library_description_free(&std_library);
}
