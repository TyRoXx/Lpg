#include "find_next_token.h"
#include "test_semantics.h"
#include "test.h"
#include "lpg_check.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"
#include <string.h>

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

static void expect_no_errors(source_location where, void *user)
{
    (void)where;
    (void)user;
    FAIL();
}

typedef struct expected_errors
{
    source_location const *locations;
    size_t count;
} expected_errors;

static void expect_errors(source_location where, void *user)
{
    expected_errors *expected = user;
    REQUIRE(expected->count >= 1);
    REQUIRE(source_location_equals(where, expected->locations[0]));
    ++expected->locations;
    --expected->count;
}

void test_semantics(void)
{
    REQUIRE(!instruction_equals(
        instruction_create_loop(instruction_sequence_create(NULL, 0)),
        instruction_create_call(call_instruction_create(0, NULL, 0, 0))));
    {
        instruction_sequence const left = instruction_sequence_create(NULL, 0);
        instruction_sequence const right = instruction_sequence_create(
            allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] =
            instruction_create_call(call_instruction_create(0, NULL, 0, 0));
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        instruction_sequence const left = instruction_sequence_create(
            allocate_array(1, sizeof(*left.elements)), 1);
        left.elements[0] =
            instruction_create_loop(instruction_sequence_create(NULL, 0));
        instruction_sequence const right = instruction_sequence_create(
            allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] =
            instruction_create_call(call_instruction_create(0, NULL, 0, 0));
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        structure const empty_global = structure_create(NULL, 0);
        sequence root = sequence_create(NULL, 0);
        checked_program checked =
            check(root, empty_global, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_unit(0);
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }

    structure_member *globals = allocate_array(3, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()), NULL, 0)),
        unicode_string_from_c_str("f"));
    globals[1] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()), NULL, 0)),
        unicode_string_from_c_str("g"));
    globals[2] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()),
                                    type_allocate(type_from_string_ref()), 1)),
        unicode_string_from_c_str("print"));
    structure const non_empty_global = structure_create(globals, 3);

    {
        sequence root = parse("f()");
        checked_program checked =
            check(root, non_empty_global, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements =
            allocate_array(3, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_global(0);
        expected_body_elements[1] = instruction_create_read_struct(
            read_struct_instruction_create(0, 0, 1));
        expected_body_elements[2] =
            instruction_create_call(call_instruction_create(1, NULL, 0, 2));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 3);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("f()\n"
                              "h()");
        source_location const expected_error_locations[] = {
            source_location_create(1, 0)};
        expected_errors expected = {expected_error_locations, 1};
        checked_program checked =
            check(root, non_empty_global, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements =
            allocate_array(3, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_global(0);
        expected_body_elements[1] = instruction_create_read_struct(
            read_struct_instruction_create(0, 0, 1));
        expected_body_elements[2] =
            instruction_create_call(call_instruction_create(1, NULL, 0, 2));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 3);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("loop\n"
                              "    f()");
        checked_program checked =
            check(root, non_empty_global, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const loop_body = allocate_array(3, sizeof(*loop_body));
        loop_body[0] = instruction_create_global(0);
        loop_body[1] = instruction_create_read_struct(
            read_struct_instruction_create(0, 0, 1));
        loop_body[2] =
            instruction_create_call(call_instruction_create(1, NULL, 0, 2));
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(instruction_sequence_create(loop_body, 3));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("loop\n"
                              "    f()\n"
                              "    g()");
        checked_program checked =
            check(root, non_empty_global, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const loop_body = allocate_array(6, sizeof(*loop_body));
        loop_body[0] = instruction_create_global(0);
        loop_body[1] = instruction_create_read_struct(
            read_struct_instruction_create(0, 0, 1));
        loop_body[2] =
            instruction_create_call(call_instruction_create(1, NULL, 0, 2));
        loop_body[3] = instruction_create_global(3);
        loop_body[4] = instruction_create_read_struct(
            read_struct_instruction_create(3, 1, 4));
        loop_body[5] =
            instruction_create_call(call_instruction_create(4, NULL, 0, 5));
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(instruction_sequence_create(loop_body, 6));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("print(\"hello, world!\")");
        checked_program checked =
            check(root, non_empty_global, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements =
            allocate_array(4, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_global(0);
        expected_body_elements[1] = instruction_create_read_struct(
            read_struct_instruction_create(0, 2, 1));
        expected_body_elements[2] =
            instruction_create_string_literal(string_literal_instruction_create(
                unicode_string_from_c_str("hello, world!"), 2));
        {
            register_id *const arguments =
                allocate_array(1, sizeof(*arguments));
            arguments[0] = 2;
            expected_body_elements[3] = instruction_create_call(
                call_instruction_create(1, arguments, 1, 3));
        }
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 4);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    structure_free(&non_empty_global);
}
