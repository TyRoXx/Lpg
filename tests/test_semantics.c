#include "test_semantics.h"
#include "test.h"
#include "lpg_check.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"
#include <string.h>
#include "handle_parse_error.h"
#include "lpg_find_next_token.h"
#include "lpg_for.h"
#include "lpg_instruction.h"
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

typedef struct expected_errors
{
    semantic_error const *errors;
    size_t count;
} expected_errors;

static void expect_errors(semantic_error const error, void *user)
{
    expected_errors *expected = user;
    REQUIRE(expected->count >= 1);
    REQUIRE(semantic_error_equals(error, expected->errors[0]));
    ++expected->errors;
    --expected->count;
}

static void check_single_wellformed_function(
    char const *const source, structure const non_empty_global,
    instruction *const expected_body_elements, size_t const expected_body_size)
{
    sequence root = parse(source);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    instruction_sequence const expected_body =
        instruction_sequence_create(expected_body_elements, expected_body_size);
    REQUIRE(instruction_sequence_equals(
        &expected_body, &checked.functions[0].body));
    checked_program_free(&checked);
    instruction_sequence_free(&expected_body);
}

void test_semantics(void)
{
    REQUIRE(!instruction_equals(
        instruction_create_loop(instruction_sequence_create(NULL, 0)),
        instruction_create_call(call_instruction_create(0, NULL, 0, 0))));
    REQUIRE(!instruction_equals(
        instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
        instruction_create_call(call_instruction_create(1, NULL, 0, 0))));
    REQUIRE(!instruction_equals(
        instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
        instruction_create_call(call_instruction_create(0, NULL, 0, 1))));
    {
        register_id arguments = 0;
        REQUIRE(!instruction_equals(
            instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
            instruction_create_call(
                call_instruction_create(0, &arguments, 1, 0))));
    }
    {
        register_id left = 0;
        register_id right = 1;
        REQUIRE(!instruction_equals(
            instruction_create_call(call_instruction_create(0, &left, 1, 0)),
            instruction_create_call(call_instruction_create(0, &right, 1, 0))));
    }
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

    standard_library_description const std_library =
        describe_standard_library();

    {
        char const *const sources[] = {"f()\n", "f()"};
        LPG_FOR(size_t, i, LPG_ARRAY_SIZE(sources))
        {
            instruction const expected_body_elements[] = {
                instruction_create_global(0),
                instruction_create_read_struct(
                    read_struct_instruction_create(0, 0, 1)),
                instruction_create_call(
                    call_instruction_create(1, NULL, 0, 2))};
            check_single_wellformed_function(
                sources[i], std_library.globals,
                LPG_COPY_ARRAY(expected_body_elements));
        }
    }
    {
        instruction const loop_body[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 0, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        check_single_wellformed_function("loop\n"
                                         "    f()",
                                         std_library.globals,
                                         expected_body_elements, 1);
    }
    {
        instruction const loop_body[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 0, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2)),
            instruction_create_global(3),
            instruction_create_read_struct(
                read_struct_instruction_create(3, 1, 4)),
            instruction_create_call(call_instruction_create(4, NULL, 0, 5))};
        instruction const expected_body_elements[] = {instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)))};
        check_single_wellformed_function(
            "loop\n"
            "    f()\n"
            "    g()",
            std_library.globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const loop_body[] = {instruction_create_break()};
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        check_single_wellformed_function("loop\n"
                                         "    break",
                                         std_library.globals,
                                         expected_body_elements, 1);
    }
    {
        instruction const loop_body[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1)),
            instruction_create_break()};
        instruction *const expected_body_elements =
            allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        expected_body_elements[1] = instruction_create_instantiate_enum(
            instantiate_enum_instruction_create(1, 0));
        check_single_wellformed_function("loop\n"
                                         "    let v = boolean.true\n"
                                         "    break\n"
                                         "let v = boolean.false\n",
                                         std_library.globals,
                                         expected_body_elements, 2);
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 2, 1)),
            instruction_create_string_literal(string_literal_instruction_create(
                unicode_string_from_c_str("hello, world!"), 2)),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "print(\"hello, world!\")", std_library.globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(2, 1)),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(boolean.true)", std_library.globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(2, 0)),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(boolean.false)", std_library.globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        sequence root = parse("f()\n"
                              "h()");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_unknown_element, source_location_create(1, 0))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 0, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("assert(boolean.something)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_unknown_element, source_location_create(0, 15))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(boolean.true.true)");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_no_members_on_enum_elements,
                                  source_location_create(0, 20))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(\"true\")");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(2)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(f)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(boolean)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(f())");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert()");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_missing_argument, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence const root = parse("assert(boolean.true, boolean.false)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_extraneous_argument, source_location_create(0, 21))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(2, 1)),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence const root = parse("and(boolean.true, boolean.false)");
        checked_program checked =
            check(root, std_library.globals, expect_no_errors, NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        register_id *const arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = 2;
        arguments[1] = 3;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 5, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(2, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(3, 0)),
            instruction_create_call(
                call_instruction_create(1, arguments, 2, 4))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("break\n");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_break_outside_of_loop,
                                  source_location_create(0, 0))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 0;
        instruction const expected_body_elements[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1)),
            instruction_create_global(1),
            instruction_create_read_struct(
                read_struct_instruction_create(1, 4, 2)),
            instruction_create_call(
                call_instruction_create(2, arguments, 1, 3))};
        check_single_wellformed_function(
            "let v = boolean.true\n"
            "assert(v)\n",
            std_library.globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 0;
        instruction const expected_body_elements[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1)),
            instruction_create_global(1),
            instruction_create_read_struct(
                read_struct_instruction_create(1, 4, 2)),
            instruction_create_call(
                call_instruction_create(2, arguments, 1, 3))};
        check_single_wellformed_function(
            "let v : boolean = boolean.true\n"
            "assert(v)\n",
            std_library.globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        sequence root = parse("let v : boolean.true = boolean.true\n");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type,
                                  source_location_create(0, 8))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("let v : f() = boolean.true\n");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type,
                                  source_location_create(0, 8))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("let v = w\n");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_unknown_element, source_location_create(0, 8))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("let v : boolean = f()\n");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 18))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 0, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("let v : boolean = boolean.true\n"
                              "let v : boolean = boolean.true\n");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_declaration_with_existing_name,
                                  source_location_create(1, 4))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library.globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(0, 1)),
            instruction_create_instantiate_enum(
                instantiate_enum_instruction_create(1, 1))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_integer_literal(
                integer_literal_instruction_create(0, integer_create(0, 1)))};
        check_single_wellformed_function(
            "let v = 1\n", std_library.globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    standard_library_description_free(&std_library);
}
