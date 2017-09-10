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
#include "lpg_standard_library.h"
#include <stdio.h>
#include "lpg_assert.h"
#include "print_instruction.h"

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
    if (!instruction_sequence_equals(
            &expected_body, &checked.functions[0].body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected_body);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(checked.functions[0].body);
        FAIL();
    }
    checked_program_free(&checked);
    instruction_sequence_free(&expected_body);
}

static void check_function(checked_function const expected,
                           checked_function const gotten)
{
    REQUIRE(expected.return_value == gotten.return_value);
    REQUIRE(expected.number_of_registers == gotten.number_of_registers);
    REQUIRE(function_pointer_equals(*expected.signature, *gotten.signature));
    if (!instruction_sequence_equals(&expected.body, &gotten.body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected.body);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(gotten.body);
        FAIL();
    }
}

static void check_wellformed_program(char const *const source,
                                     structure const non_empty_global,
                                     checked_program const expected)
{
    sequence root = parse(source);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == expected.function_count);
    for (size_t i = 0; i < expected.function_count; ++i)
    {
        check_function(expected.functions[i], checked.functions[i]);
    }
    checked_program_free(&checked);
    checked_program_free(&expected);
}

static void test_loops(const standard_library_description *std_library);
static void test_printing(const standard_library_description *std_library);
static void test_assert(const standard_library_description *std_library);
static void test_let_assignments(const standard_library_description *std_library);
static void test_functions(const standard_library_description *std_library);

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
        expected_body_elements[0] = instruction_create_literal(
            literal_instruction_create(0, value_from_unit()));
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
        char const *const sources[] = {"read()\n", "read()", "/*comment*/\n"
                                                             "read()\n",
                                       "//comment\n"
                                       "read()\n"};
        LPG_FOR(size_t, i, LPG_ARRAY_SIZE(sources))
        {
            instruction const expected_body_elements[] = {
                instruction_create_global(0),
                instruction_create_read_struct(
                    read_struct_instruction_create(0, 10, 1)),
                instruction_create_call(
                    call_instruction_create(1, NULL, 0, 2))};
            check_single_wellformed_function(
                sources[i], std_library.globals,
                LPG_COPY_ARRAY(expected_body_elements));
        }
    }
    test_loops(&std_library);
    test_printing(&std_library);

    {
        value state = value_from_integer(integer_create(0, 123));
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_enum_element(1, &state))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        check_single_wellformed_function(
            "let s = option.some(123)", std_library.globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        sequence root = parse("read()\n"
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
                read_struct_instruction_create(0, 10, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2)),
            instruction_create_literal(
                literal_instruction_create(3, value_from_unit()))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    test_assert(&std_library);
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
            instruction_create_literal(literal_instruction_create(
                0, value_from_enum_element(1, NULL))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("let v : read() = boolean.true\n");
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
            instruction_create_literal(literal_instruction_create(
                0, value_from_enum_element(1, NULL))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
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
        sequence root = parse("let v : boolean = read()\n");
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
                read_struct_instruction_create(0, 10, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2)),
            instruction_create_literal(
                literal_instruction_create(3, value_from_unit()))};
        instruction_sequence const expected_body =
            instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    test_let_assignments(&std_library);
    test_functions(&std_library);
    standard_library_description_free(&std_library);
}

static void test_functions(const standard_library_description *std_library)
{
    {
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(
                       function_pointer_value_from_internal(1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        instruction const expected_lambda[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 123))))};
        checked_program const expected = {
            {NULL}, allocate_array(2, sizeof(*expected.functions)), 2};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[0] = checked_function_create(
                1, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_main)), 2);
        }
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(
                type_from_integer_range(integer_range_create(
                    integer_create(0, 123), integer_create(0, 123))),
                NULL, 0);
            expected.functions[1] = checked_function_create(
                0, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                1);
        }
        check_wellformed_program(
            "let f = () 123\n", (*std_library).globals, expected);
    }
    {
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(
                       function_pointer_value_from_internal(1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_lambda[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, NULL))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        checked_program const expected = {
            {NULL}, allocate_array(2, sizeof(*expected.functions)), 2};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[0] = checked_function_create(
                1, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_main)), 2);
        }
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[1] = checked_function_create(
                3, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                4);
        }
        check_wellformed_program("let f = ()\n"
                                 "    assert(boolean.true)\n",
                                 (*std_library).globals, expected);
    }
    {
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(
                       function_pointer_value_from_internal(1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        instruction const expected_lambda[] = {
            instruction_create_literal(literal_instruction_create(
                1, value_from_integer(integer_create(0, 123))))};
        checked_program const expected = {
            {NULL}, allocate_array(2, sizeof(*expected.functions)), 2};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[0] = checked_function_create(
                1, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_main)), 2);
        }
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            type *const parameters = allocate_array(1, sizeof(*parameters));
            parameters[0] =
                type_from_enumeration(&(*std_library).stable->boolean);
            *signature = function_pointer_create(
                type_from_integer_range(integer_range_create(
                    integer_create(0, 123), integer_create(0, 123))),
                parameters, 1);
            expected.functions[1] = checked_function_create(
                1, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                2);
        }
        check_wellformed_program(
            "let f = (a: boolean) 123\n", (*std_library).globals, expected);
    }
    {
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(
                       function_pointer_value_from_internal(1)))),
            instruction_create_literal(literal_instruction_create(
                1, value_from_enum_element(1, NULL))),
            instruction_create_literal(
                literal_instruction_create(2, value_from_unit()))};
        instruction const expected_lambda[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 3, 1))};
        checked_program const expected = {
            {NULL}, allocate_array(2, sizeof(*expected.functions)), 2};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[0] = checked_function_create(
                2, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_main)), 3);
        }
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_type(), NULL, 0);
            expected.functions[1] = checked_function_create(
                1, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                2);
        }
        check_wellformed_program("let f = () boolean\n"
                                 "let v : f() = boolean.true",
                                 (*std_library).globals, expected);
    }
    {
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(
                       function_pointer_value_from_internal(1)))),
            instruction_create_literal(literal_instruction_create(
                1, value_from_enum_element(1, NULL))),
            instruction_create_literal(
                literal_instruction_create(2, value_from_unit()))};
        checked_program const expected = {
            {NULL}, allocate_array(2, sizeof(*expected.functions)), 2};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(type_from_unit(), NULL, 0);
            expected.functions[0] = checked_function_create(
                2, signature,
                instruction_sequence_create(LPG_COPY_ARRAY(expected_main)), 3);
        }
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            type *const parameters = allocate_array(1, sizeof(*parameters));
            parameters[0] = type_from_type();
            *signature =
                function_pointer_create(type_from_type(), parameters, 1);
            expected.functions[1] = checked_function_create(
                0, signature, instruction_sequence_create(NULL, 0), 1);
        }
        check_wellformed_program("let f = (a: type) a\n"
                                 "let v : f(boolean) = boolean.true",
                                 (*std_library).globals, expected);
    }
    {
        register_id *values = allocate_array(2, sizeof(*values));
        values[0] = 0;
        values[1] = 1;
        instruction const expected_main_function[] = {
            instruction_create_literal(literal_instruction_create(
                values[0], value_from_enum_element(0, NULL))),
            instruction_create_literal(literal_instruction_create(
                values[1], value_from_enum_element(1, NULL))),
            instruction_create_tuple(tuple_instruction_create(values, 2, 2)),
            instruction_create_read_struct(
                read_struct_instruction_create(2, 1, 3)),
            instruction_create_literal(
                literal_instruction_create(4, value_from_unit()))};
        check_single_wellformed_function(
            "let t = {boolean.false, boolean.true}\n"
            "let u = t.1\n",
            (*std_library).globals, LPG_COPY_ARRAY(expected_main_function));
    }
}

static void test_printing(const standard_library_description *std_library)
{
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 2, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_string_ref(
                       unicode_view_from_c_str("hello, world!")))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "print(\"hello, world!\")", (*std_library).globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 2, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_string_ref(
                       unicode_view_from_c_str("hello, world!")))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "print(concat(\"hello, \", \"world!\"))", (*std_library).globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
}

static void test_loops(const standard_library_description *std_library)
{
    {
        instruction const loop_body[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 10, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction *const expected_body_elements =
            allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        expected_body_elements[1] = instruction_create_literal(
            literal_instruction_create(3, value_from_unit()));
        check_single_wellformed_function("loop\n"
                                         "    read()",
                                         (*std_library).globals,
                                         expected_body_elements, 2);
    }
    {
        instruction const loop_body[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 10, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2)),
            instruction_create_global(3),
            instruction_create_read_struct(
                read_struct_instruction_create(3, 10, 4)),
            instruction_create_call(call_instruction_create(4, NULL, 0, 5))};
        instruction const expected_body_elements[] = {
            instruction_create_loop(
                instruction_sequence_create(LPG_COPY_ARRAY(loop_body))),
            instruction_create_literal(
                literal_instruction_create(6, value_from_unit()))};
        check_single_wellformed_function(
            "loop\n"
            "    read()\n"
            "    read()",
            (*std_library).globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const loop_body[] = {instruction_create_break()};
        instruction *const expected_body_elements =
            allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        expected_body_elements[1] = instruction_create_literal(
            literal_instruction_create(0, value_from_unit()));
        check_single_wellformed_function("loop\n"
                                         "    break",
                                         (*std_library).globals,
                                         expected_body_elements, 2);
    }
    {
        instruction const loop_body[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_enum_element(1, NULL))),
            instruction_create_break()};
        instruction *const expected_body_elements =
            allocate_array(3, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_loop(
            instruction_sequence_create(LPG_COPY_ARRAY(loop_body)));
        expected_body_elements[1] = instruction_create_literal(
            literal_instruction_create(1, value_from_enum_element(0, NULL)));
        expected_body_elements[2] = instruction_create_literal(
            literal_instruction_create(2, value_from_unit()));
        check_single_wellformed_function("loop\n"
                                         "    let v = boolean.true\n"
                                         "    break\n"
                                         "let v = boolean.false\n",
                                         (*std_library).globals,
                                         expected_body_elements, 3);
    }
    {
        sequence root = parse("break\n");
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_break_outside_of_loop,
                                  source_location_create(0, 0))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
}

static void test_assert(const standard_library_description *std_library)
{
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, NULL))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(boolean.true)", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(0, NULL))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(boolean.false)", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(0, NULL))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(not(boolean.true))", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        sequence root = parse("assert(boolean.something)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_unknown_element, source_location_create(0, 15))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(assert)");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("assert(read())");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
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
            check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, NULL))),
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
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0),
            instruction_create_read_struct(
                read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, NULL))),
            instruction_create_call(
                call_instruction_create(1, arguments, 1, 3))};
        check_single_wellformed_function(
            "assert(string-equals(\"\", \"\"))", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
}

static void
test_let_assignments(const standard_library_description *std_library)
{

    {
        sequence root = parse("let v : boolean = boolean.true\n"
                                      "let v : boolean = boolean.true\n");
        semantic_error const errors[] = {
                semantic_error_create(semantic_error_declaration_with_existing_name,
                                      source_location_create(1, 4))};
        expected_errors expected = {errors, 1};
        checked_program checked =
                check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
                instruction_create_literal(literal_instruction_create(
                        0, value_from_enum_element(1, NULL))),
                instruction_create_literal(literal_instruction_create(
                        1, value_from_enum_element(1, NULL))),
                instruction_create_literal(
                        literal_instruction_create(2, value_from_unit()))};
        instruction_sequence const expected_body =
                instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(
                &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        check_single_wellformed_function(
            "let v = 1\n", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        check_single_wellformed_function(
            "let v : int(1, 10) = 1\n", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)))),
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit()))};
        check_single_wellformed_function(
            "let v : int(10, 1) = 1\n", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        sequence root = parse("let v : int(1, 10) = 11\n");
        semantic_error const errors[] = {semantic_error_create(
            semantic_error_type_mismatch, source_location_create(0, 21))};
        expected_errors expected = {errors, 1};
        checked_program checked =
            check(root, std_library->globals, expect_errors, &expected);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 2);
        checked_program_free(&checked);
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(
                literal_instruction_create(1, value_from_unit())),
            instruction_create_literal(
                literal_instruction_create(2, value_from_unit()))};
        check_single_wellformed_function(
            "let v : unit = unit_value\n", std_library->globals,
            LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 0;
        instruction const expected_body_elements[] = {
                instruction_create_literal(literal_instruction_create(
                        0, value_from_enum_element(1, NULL))),
                instruction_create_global(1),
                instruction_create_read_struct(
                        read_struct_instruction_create(1, 4, 2)),
                instruction_create_call(
                        call_instruction_create(2, arguments, 1, 3))};
        check_single_wellformed_function(
                "let v = boolean.true\n"
                        "assert(v)\n",
                std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 0;
        instruction const expected_body_elements[] = {
                instruction_create_literal(literal_instruction_create(
                        0, value_from_enum_element(1, NULL))),
                instruction_create_global(1),
                instruction_create_read_struct(
                        read_struct_instruction_create(1, 4, 2)),
                instruction_create_call(
                        call_instruction_create(2, arguments, 1, 3))};
        check_single_wellformed_function(
                "let v : boolean = boolean.true\n"
                        "assert(v)\n",
                std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
}