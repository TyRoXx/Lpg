#include "test_interprete.h"
#include "test.h"
#include "lpg_interprete.h"
#include <string.h>
#include "lpg_allocate.h"
#include "lpg_stream_writer.h"
#include "lpg_assert.h"
#include "handle_parse_error.h"
#include "lpg_find_next_token.h"
#include "lpg_check.h"
#include "lpg_structure_member.h"
#include "standard_library.h"

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

static value assert_impl(value const *arguments, void *environment)
{
    (void)environment;
    enum_element_id const argument = arguments[0].enum_element;
    REQUIRE(argument == 1);
    return value_from_unit();
}

static value and_impl(value const *arguments, void *environment)
{
    (void)environment;
    enum_element_id const left = arguments[0].enum_element;
    enum_element_id const right = arguments[1].enum_element;
    return value_from_enum_element(left && right);
}

static value or_impl(value const *arguments, void *environment)
{
    (void)environment;
    enum_element_id const left = arguments[0].enum_element;
    enum_element_id const right = arguments[1].enum_element;
    return value_from_enum_element(left || right);
}

static value not_impl(value const *arguments, void *environment)
{
    (void)environment;
    enum_element_id const argument = arguments[0].enum_element;
    return value_from_enum_element(!argument);
}

static void expect_output(char const *source, char const *output,
                          structure const global_object)
{
    memory_writer print_buffer = {NULL, 0, 0};
    stream_writer print_destination = memory_writer_erase(&print_buffer);
    value const globals_values[8] = {
        /*f*/ value_from_unit(),
        /*g*/ value_from_unit(),
        /*print*/ value_from_function_pointer(
            function_pointer_value_from_external(print, &print_destination)),
        /*boolean*/ value_from_unit(),
        /*assert*/ value_from_function_pointer(
            function_pointer_value_from_external(assert_impl, NULL)),
        /*and*/ value_from_function_pointer(
            function_pointer_value_from_external(and_impl, NULL)),
        /*or*/ value_from_function_pointer(
            function_pointer_value_from_external(or_impl, NULL)),
        /*not*/ value_from_function_pointer(
            function_pointer_value_from_external(not_impl, NULL))};
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
    standard_library_description const std_library =
        describe_standard_library();

    expect_output("", "", std_library.globals);
    expect_output("print(\"\")", "", std_library.globals);
    expect_output(
        "print(\"Hello, world!\")", "Hello, world!", std_library.globals);
    expect_output(
        "print(\"Hello, world!\")\n", "Hello, world!", std_library.globals);
    expect_output(
        "print(\"Hello, world!\")\r\n", "Hello, world!", std_library.globals);
    expect_output("print(\"Hello, \")\nprint(\"world!\")", "Hello, world!",
                  std_library.globals);
    expect_output("loop\n"
                  "    let v = \"Hello, world!\"\n"
                  "    print(v)\n"
                  "    break",
                  "Hello, world!", std_library.globals);
    expect_output("assert(boolean.true)", "", std_library.globals);
    expect_output("assert(not(boolean.false))", "", std_library.globals);
    expect_output(
        "assert(and(boolean.true, boolean.true))", "", std_library.globals);
    expect_output(
        "assert(or(boolean.true, boolean.true))", "", std_library.globals);
    expect_output(
        "assert(or(boolean.false, boolean.true))", "", std_library.globals);
    expect_output(
        "assert(or(boolean.true, boolean.false))", "", std_library.globals);

    standard_library_description_free(&std_library);
}
