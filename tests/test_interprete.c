#include "test_interprete.h"
#include "test.h"
#include "lpg_interprete.h"
#include <string.h>
#include "lpg_allocate.h"
#include "lpg_stream_writer.h"
#include "lpg_assert.h"
#include "handle_parse_error.h"
#include "lpg_find_next_token.h"

static sequence parse(char const *input)
{
    test_parser_user user = {
        {input, strlen(input), source_location_create(0, 0)}, NULL, 0};
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

static value print(value const *arguments, void *environment)
{
    unicode_view const text = arguments[0].string_ref;
    stream_writer *destination = environment;
    REQUIRE(stream_writer_write_bytes(*destination, text.begin, text.length) ==
            success);
    return value_from_unit();
}

static void expect_output(char const *source, char const *output,
                          structure const global_object)
{
    memory_writer print_buffer = {NULL, 0, 0};
    stream_writer print_destination = memory_writer_erase(&print_buffer);
    value const globals_values[1] = {value_from_function_pointer(
        function_pointer_value_from_external(print, &print_destination))};
    sequence root = parse(source);
    checked_program checked =
        check(root, global_object, expect_no_errors, NULL);
    sequence_free(&root);
    interprete(checked, globals_values);
    checked_program_free(&checked);
    REQUIRE(memory_writer_equals(print_buffer, output));
    memory_writer_free(&print_buffer);
}

void test_interprete(void)
{
    structure_member *const globals = allocate_array(1, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()),
                                    type_allocate(type_from_string_ref()), 1)),
        unicode_string_from_c_str("print"));
    structure const global_object = structure_create(globals, 1);

    expect_output("", "", global_object);
    expect_output("print(\"\")", "", global_object);
    expect_output("print(\"Hello, world!\")", "Hello, world!", global_object);
    expect_output("print(\"Hello, world!\")\n", "Hello, world!", global_object);
    expect_output(
        "print(\"Hello, world!\")\r\n", "Hello, world!", global_object);
    expect_output("print(\"Hello, \")\nprint(\"world!\")", "Hello, world!",
                  global_object);
    expect_output("loop\n"
                  "    print(\"Hello, world!\")\n"
                  "    break",
                  "Hello, world!", global_object);

    structure_free(&global_object);
}
