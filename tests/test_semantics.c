#include "test_semantics.h"
#include "find_builtin_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_instruction.h"
#include "lpg_standard_library.h"
#include "lpg_structure_member.h"
#include "print_instruction.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

static sequence parse(char const *input)
{
    test_parser_user user = {{input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(find_next_token, &user, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(complete_semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void check_single_wellformed_function(char const *const source, structure const non_empty_global,
                                             instruction *const expected_body_elements, size_t const expected_body_size)
{
    sequence root = parse(source);
    unicode_string const module_directory = find_builtin_module_directory();
    module_loader loader =
        module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, &loader,
              source_file_create(unicode_view_from_c_str("test.lpg"), unicode_view_from_c_str(source)), NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    instruction_sequence const expected_body = instruction_sequence_create(expected_body_elements, expected_body_size);
    if (!instruction_sequence_equals(&expected_body, &checked.functions[0].body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected_body, 0);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(checked.functions[0].body, 0);
        FAIL();
    }
    checked_program_free(&checked);
    unicode_string_free(&module_directory);
    instruction_sequence_free(&expected_body);
}

static void test_function(checked_function const expected, checked_function const gotten)
{
    REQUIRE(expected.number_of_registers == gotten.number_of_registers);
    for (register_id i = 0; i < expected.number_of_registers; ++i)
    {
        REQUIRE(unicode_string_equals(expected.register_debug_names[i], gotten.register_debug_names[i]));
    }
    REQUIRE(function_pointer_equals(*expected.signature, *gotten.signature));
    if (!instruction_sequence_equals(&expected.body, &gotten.body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected.body, 0);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(gotten.body, 0);
        FAIL();
    }
}

static void check_wellformed_program(char const *const source, structure const non_empty_global,
                                     checked_program const expected)
{
    sequence root = parse(source);
    unicode_string const module_directory = find_builtin_module_directory();
    module_loader loader =
        module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, &loader,
              source_file_create(unicode_view_from_c_str("test.lpg"), unicode_view_from_c_str(source)), NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == expected.function_count);
    for (size_t i = 0; i < expected.function_count; ++i)
    {
        test_function(expected.functions[i], checked.functions[i]);
    }
    checked_program_free(&checked);
    checked_program_free(&expected);
    unicode_string_free(&module_directory);
}

static void test_loops(const standard_library_description *std_library);
static void test_assert(const standard_library_description *std_library);
static void test_let_assignments(const standard_library_description *std_library);
static void test_functions(const standard_library_description *std_library);

void test_semantics(void)
{
    REQUIRE(
        !instruction_equals(instruction_create_loop(loop_instruction_create(0, instruction_sequence_create(NULL, 0))),
                            instruction_create_call(call_instruction_create(0, NULL, 0, 0))));
    REQUIRE(!instruction_equals(instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
                                instruction_create_call(call_instruction_create(1, NULL, 0, 0))));
    REQUIRE(!instruction_equals(instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
                                instruction_create_call(call_instruction_create(0, NULL, 0, 1))));
    {
        register_id arguments = 0;
        REQUIRE(!instruction_equals(instruction_create_call(call_instruction_create(0, NULL, 0, 0)),
                                    instruction_create_call(call_instruction_create(0, &arguments, 1, 0))));
    }
    {
        register_id left = 0;
        register_id right = 1;
        REQUIRE(!instruction_equals(instruction_create_call(call_instruction_create(0, &left, 1, 0)),
                                    instruction_create_call(call_instruction_create(0, &right, 1, 0))));
    }
    {
        instruction_sequence const left = instruction_sequence_create(NULL, 0);
        instruction_sequence const right = instruction_sequence_create(allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] = instruction_create_call(call_instruction_create(0, NULL, 0, 0));
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        instruction_sequence const left = instruction_sequence_create(allocate_array(1, sizeof(*left.elements)), 1);
        left.elements[0] = instruction_create_loop(loop_instruction_create(0, instruction_sequence_create(NULL, 0)));
        instruction_sequence const right = instruction_sequence_create(allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] = instruction_create_call(call_instruction_create(0, NULL, 0, 0));
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        structure const empty_global = structure_create(NULL, 0);
        sequence root = sequence_create(NULL, 0);
        unicode_string const module_directory = find_builtin_module_directory();
        module_loader loader =
            module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
        checked_program checked =
            check(root, empty_global, expect_no_errors, &loader,
                  source_file_create(unicode_view_from_c_str("test.lpg"), unicode_view_from_c_str("")), NULL);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements = allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_literal(literal_instruction_create(0, value_from_unit(), type_from_unit()));
        expected_body_elements[1] = instruction_create_return(return_instruction_create(0, 1));
        instruction_sequence const expected_body = instruction_sequence_create(expected_body_elements, 2);
        REQUIRE(instruction_sequence_equals(&expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
        unicode_string_free(&module_directory);
    }

    standard_library_description const std_library = describe_standard_library();

    test_loops(&std_library);
    test_let_assignments(&std_library);
    test_functions(&std_library);
    test_assert(&std_library);
    standard_library_description_free(&std_library);
}

static type make_integer_constant_type(integer const constant)
{
    return type_from_integer_range(integer_range_create(constant, constant));
}

static void test_functions(const standard_library_description *std_library)
{
    {
        function_pointer *const signature_lambda = allocate(sizeof(*signature_lambda));
        {
            *signature_lambda = function_pointer_create(
                optional_type_create_set(
                    type_from_integer_range(integer_range_create(integer_create(0, 123), integer_create(0, 123)))),
                tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());
        }
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(function_pointer_value_from_internal(1, NULL, 0)),
                type_from_function_pointer(signature_lambda))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        instruction const expected_lambda[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 123)), make_integer_constant_type(integer_create(0, 123)))),
            instruction_create_return(return_instruction_create(0, 1))};
        checked_program const expected = {
            NULL, 0,    NULL, 0, garbage_collector_create(SIZE_MAX), allocate_array(2, sizeof(*expected.functions)),
            2,    NULL, 0};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                                 tuple_type_create(NULL, 0), optional_type_create_empty());
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str("f"), unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[0] =
                checked_function_create(signature, instruction_sequence_create(LPG_COPY_ARRAY(expected_main)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        {
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[1] =
                checked_function_create(signature_lambda, instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        check_wellformed_program("let f = () 123\n", (*std_library).globals, expected);
    }
    {
        function_pointer *const signature_lambda = allocate(sizeof(*signature_lambda));
        *signature_lambda = function_pointer_create(
            optional_type_create_set(
                type_from_integer_range(integer_range_create(integer_create(0, 123), integer_create(0, 123)))),
            tuple_type_create(NULL, 0), tuple_type_create(NULL, 0), optional_type_create_empty());
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(function_pointer_value_from_internal(1, NULL, 0)),
                type_from_function_pointer(signature_lambda))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        literal_instruction literal = literal_instruction_create(
            0, value_from_integer(integer_create(0, 123)), make_integer_constant_type(integer_create(0, 123)));
        instruction const expected_lambda[] = {
            instruction_create_literal(literal), instruction_create_return(return_instruction_create(literal.into, 1))};
        checked_program const expected = {
            NULL, 0,    NULL, 0, garbage_collector_create(SIZE_MAX), allocate_array(2, sizeof(*expected.functions)),
            2,    NULL, 0};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                                 tuple_type_create(NULL, 0), optional_type_create_empty());
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str("f"), unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[0] =
                checked_function_create(signature, instruction_sequence_create(LPG_COPY_ARRAY(expected_main)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        {
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[1] =
                checked_function_create(signature_lambda, instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        check_wellformed_program("let f = (): int(123, 123)\n"
                                 "    return 123",
                                 (*std_library).globals, expected);
    }
    {
        function_pointer *const signature_lambda = allocate(sizeof(*signature_lambda));
        *signature_lambda =
            function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                    tuple_type_create(NULL, 0), optional_type_create_empty());
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(function_pointer_value_from_internal(1, NULL, 0)),
                type_from_function_pointer(signature_lambda))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_lambda[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        checked_program const expected = {
            NULL, 0,    NULL, 0, garbage_collector_create(SIZE_MAX), allocate_array(2, sizeof(*expected.functions)),
            2,    NULL, 0};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                                 tuple_type_create(NULL, 0), optional_type_create_empty());
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str("f"), unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[0] =
                checked_function_create(signature, instruction_sequence_create(LPG_COPY_ARRAY(expected_main)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        {
            unicode_string const register_debug_names[] = {unicode_string_from_c_str(""), unicode_string_from_c_str(""),
                                                           unicode_string_from_c_str(""), unicode_string_from_c_str(""),
                                                           unicode_string_from_c_str("")};
            expected.functions[1] =
                checked_function_create(signature_lambda, instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        check_wellformed_program("let f = ()\n"
                                 "    assert(boolean.true)\n",
                                 (*std_library).globals, expected);
    }
    {
        function_pointer *const signature_lambda = allocate(sizeof(*signature_lambda));
        {
            type *const parameters = allocate_array(1, sizeof(*parameters));
            parameters[0] = type_from_enumeration(0);
            *signature_lambda = function_pointer_create(
                optional_type_create_set(
                    type_from_integer_range(integer_range_create(integer_create(0, 123), integer_create(0, 123)))),
                tuple_type_create(parameters, 1), tuple_type_create(NULL, 0), optional_type_create_empty());
        }
        instruction const expected_main[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(function_pointer_value_from_internal(1, NULL, 0)),
                type_from_function_pointer(signature_lambda))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        instruction const expected_lambda[] = {
            instruction_create_literal(literal_instruction_create(
                1, value_from_integer(integer_create(0, 123)), make_integer_constant_type(integer_create(0, 123)))),
            instruction_create_return(return_instruction_create(1, 2))};
        checked_program const expected = {
            NULL, 0,    NULL, 0, garbage_collector_create(SIZE_MAX), allocate_array(2, sizeof(*expected.functions)),
            2,    NULL, 0};
        {
            function_pointer *const signature = allocate(sizeof(*signature));
            *signature = function_pointer_create(optional_type_create_set(type_from_unit()), tuple_type_create(NULL, 0),
                                                 tuple_type_create(NULL, 0), optional_type_create_empty());
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str("f"), unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[0] =
                checked_function_create(signature, instruction_sequence_create(LPG_COPY_ARRAY(expected_main)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        {
            unicode_string const register_debug_names[] = {
                unicode_string_from_c_str("a"), unicode_string_from_c_str(""), unicode_string_from_c_str("")};
            expected.functions[1] =
                checked_function_create(signature_lambda, instruction_sequence_create(LPG_COPY_ARRAY(expected_lambda)),
                                        LPG_COPY_ARRAY(register_debug_names));
        }
        check_wellformed_program("let f = (a: boolean) 123\n", (*std_library).globals, expected);
    }
}

static void test_loops(const standard_library_description *std_library)
{
    {
        instruction const loop_body[] = {instruction_create_global(0),
                                         instruction_create_read_struct(read_struct_instruction_create(0, 0, 1)),
                                         instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction *const expected_body_elements = allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(loop_instruction_create(3, instruction_sequence_create(LPG_COPY_ARRAY(loop_body))));
        expected_body_elements[1] = instruction_create_return(return_instruction_create(3, 4));
        check_single_wellformed_function("loop\n"
                                         "    side_effect()",
                                         (*std_library).globals, expected_body_elements, 2);
    }
    {
        instruction const loop_body[] = {instruction_create_global(0),
                                         instruction_create_read_struct(read_struct_instruction_create(0, 0, 1)),
                                         instruction_create_call(call_instruction_create(1, NULL, 0, 2)),
                                         instruction_create_global(3),
                                         instruction_create_read_struct(read_struct_instruction_create(3, 0, 4)),
                                         instruction_create_call(call_instruction_create(4, NULL, 0, 5))};
        instruction const expected_body_elements[] = {
            instruction_create_loop(loop_instruction_create(6, instruction_sequence_create(LPG_COPY_ARRAY(loop_body)))),
            instruction_create_return(return_instruction_create(6, 7))};
        check_single_wellformed_function("loop\n"
                                         "    side_effect()\n"
                                         "    side_effect()",
                                         (*std_library).globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const loop_body[] = {instruction_create_break(0)};
        instruction *const expected_body_elements = allocate_array(2, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(loop_instruction_create(1, instruction_sequence_create(LPG_COPY_ARRAY(loop_body))));
        expected_body_elements[1] = instruction_create_return(return_instruction_create(1, 2));
        check_single_wellformed_function("loop\n"
                                         "    break",
                                         (*std_library).globals, expected_body_elements, 2);
    }
}

static void test_assert(const standard_library_description *std_library)
{
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        check_single_wellformed_function(
            "assert(boolean.true)", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(0, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        check_single_wellformed_function(
            "assert(boolean.false)", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(0, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        check_single_wellformed_function(
            "assert(not(boolean.true))", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        check_single_wellformed_function(
            "assert(string-equals(\"\", \"\"))", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
}

static void test_let_assignments(const standard_library_description *std_library)
{
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)), make_integer_constant_type(integer_create(0, 1)))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        check_single_wellformed_function("let v = 1\n", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)), make_integer_constant_type(integer_create(0, 1)))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        check_single_wellformed_function(
            "let v : int(1, 10) = 1\n", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
    {
        instruction const expected_body_elements[] = {
            instruction_create_literal(literal_instruction_create(
                0, value_from_integer(integer_create(0, 1)), make_integer_constant_type(integer_create(0, 1)))),
            instruction_create_literal(literal_instruction_create(1, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(1, 2))};
        check_single_wellformed_function(
            "let v : int(10, 1) = 1\n", std_library->globals, LPG_COPY_ARRAY(expected_body_elements));
    }
}
