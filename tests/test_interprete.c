#include "test_interprete.h"
#include "test.h"
#include "lpg_interprete.h"
#include "find_next_token.h"
#include <string.h>
#include "lpg_allocate.h"
#include "lpg_stream_writer.h"
#include "lpg_assert.h"

static sequence parse(char const *input)
{
    test_parser_user user = {
        input, strlen(input), NULL, 0, source_location_create(0, 0)};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.remaining_size == 0);
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

void test_interprete(void)
{
    structure_member *globals_type = allocate_array(1, sizeof(*globals_type));
    globals_type[0] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()),
                                    type_allocate(type_from_string_ref()), 1)),
        unicode_string_from_c_str("print"));
    structure const non_empty_global = structure_create(globals_type, 1);

    sequence root = parse("print(\"Hello, \")\nprint(\"world!\")");
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    structure_free(&non_empty_global);
    memory_writer print_buffer = {NULL, 0};
    stream_writer print_destination = memory_writer_erase(&print_buffer);
    value const globals_values[1] = {value_from_function_pointer(
        function_pointer_value_from_external(print, &print_destination))};
    interprete(checked, globals_values);
    checked_program_free(&checked);
    REQUIRE(memory_writer_equals(print_buffer, "Hello, world!"));
    memory_writer_free(&print_buffer);
}
