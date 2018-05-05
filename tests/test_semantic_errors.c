#include "test_semantic_errors.h"
#include "test.h"
#include "lpg_check.h"
#include "lpg_allocate.h"
#include <string.h>
#include "handle_parse_error.h"
#include "lpg_instruction.h"
#include "lpg_standard_library.h"
#include "find_builtin_module_directory.h"

static sequence parse(char const *input)
{
    test_parser_user user = {{input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(find_next_token, &user, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

typedef struct expected_errors
{
    semantic_error const *errors;
    size_t count;
    unicode_view file_name;
    unicode_view source;
} expected_errors;

static expected_errors make_expected_errors(semantic_error const *const errors, size_t const count)
{
    expected_errors const result = {errors, count, unicode_view_from_c_str(""), unicode_view_from_c_str("")};
    return result;
}

static void expect_errors(complete_semantic_error const error, void *user)
{
    expected_errors *expected = user;
    REQUIRE(expected->count >= 1);
    semantic_error const expected_error = expected->errors[0];
    REQUIRE(complete_semantic_error_equals(
        error, complete_semantic_error_create(expected_error, expected->file_name, expected->source)));
    ++expected->errors;
    --expected->count;
}

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static checked_program simple_check(char const *const source, structure const global, expected_errors *const user,
                                    unicode_view const module_directory)
{
    user->file_name = unicode_view_from_c_str("test.lpg");
    user->source = unicode_view_from_c_str(source);
    sequence const root = parse(source);
    module_loader loader = module_loader_create(module_directory, expect_no_complete_parse_error, NULL);
    checked_program const result = check(root, global, expect_errors, &loader, user->file_name, user->source, user);
    sequence_free(&root);
    return result;
}

void test_semantic_errors(void)
{
    unicode_string const module_directory = find_builtin_module_directory();
    unicode_view const module_directory_view = unicode_view_from_string(module_directory);
    standard_library_description const std_library = describe_standard_library();

    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 0))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("xor(boolean.true, boolean.false)\n"
                                               "let xor = (left: boolean, right: boolean) assert(boolean.false)\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }

    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(3, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = boolean.true\n"
                                               "match s\n"
                                               "    case boolean.false: s\n"
                                               "    case 123: s\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(3, 24))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = boolean.true\n"
                                               "match s\n"
                                               "    case boolean.true: s\n"
                                               "    case boolean.false: 123\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_match_case, source_location_create(1, 0))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = boolean.true\n"
                                               "match s\n"
                                               "    case boolean.true: s\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_match_case, source_location_create(3, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = boolean.true\n"
                                               "match s\n"
                                               "    case boolean.true: s\n"
                                               "    case boolean.true: 123\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_match_case, source_location_create(1, 0))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = boolean.true\n"
                                               "match s\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 14))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = match a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = match boolean.true\n"
                                               "    case boolean.false: 1\n"
                                               "    case a: 2\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 23))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = match boolean.true\n"
                                               "    case boolean.false: 1\n"
                                               "    case boolean.true: a\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_match_unsupported, source_location_create(2, 4))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum\n"
                                               "let f = (arg: e)\n"
                                               "    match arg\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_enum_element, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum\n"
                                               "    a\n"
                                               "    a\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 6))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum\n"
                                               "    a(u)\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(3, 6))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = 0\n"
                                               "let e = enum\n"
                                               "    a\n"
                                               "    b(i)\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(4, 6))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = ()\n"
                                               "    side-effect()\n"
                                               "    boolean\n"
                                               "let e = enum\n"
                                               "    a(f())\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_extraneous_argument, source_location_create(2, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum[A]\n"
                                               "    a\n"
                                               "let x = e[boolean, boolean]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_argument, source_location_create(2, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum[A]\n"
                                               "    a\n"
                                               "let x = e[]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 6))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum[A]\n"
                                               "    a\n"
                                               "    b(unknown)\n"
                                               "let x = e[boolean]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(5, 6))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = ()\n"
                                               "    side-effect()\n"
                                               "    boolean\n"
                                               "let g = f()\n"
                                               "let e = enum[A]\n"
                                               "    b(g)\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_generic_type, source_location_create(3, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = ()\n"
                                               "    side-effect()\n"
                                               "    1\n"
                                               "let x = f()[boolean]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_generic_type, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let x = boolean[boolean]\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 10))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum[X]\n"
                                               "let x = e[unknown]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_value, source_location_create(1, 10))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let e = enum[X]\n"
                                               "let x = e[side-effect()]\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let x = u[boolean]\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 0))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("side-effect()\n"
                                               "h()",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 18, 1)),
            instruction_create_call(call_instruction_create(1, NULL, 0, 2))};
        instruction_sequence const expected_body = instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(&expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check(
            "let v : boolean.true = boolean.true\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check(
            "let v : side-effect() = boolean.true\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = w\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 18))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v : boolean = side-effect()\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 20))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v : int(0, 1) = side-effect()\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = {}.0\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = {}.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = {}.9999999999999999999999999999999999999999\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = {a}\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = () a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = w.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 19))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = unit_value.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 15))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = assert.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 2))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = () 1\n"
                                               "v.a\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = \"\".a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 10))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = 1.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 24))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let v = std.option.some.a\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 13))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = unit.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 19))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = string-ref.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 13))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = type.a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 18))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = int(0, 1).a\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(0, 20))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = (a: type) a.true\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(0, 21))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = (a: unit, b: 1) 2\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 17))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "let f = (a: i) a.unknown()\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 2))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "i.unknown()\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 24))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    method(): unit\n"
                                               "let f = (a: i) a.method.unknown()\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    a(): unit\n"
                                               "    b(): unknown\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(2, 2))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "let f = (a: i)\n"
                                               "f(1)",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(1, 12))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "let e : i = 1\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): error\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(1, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): side-effect()\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(3, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "impl i for unit\n"
                                               "    f(): 1\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(4, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "impl i for unit\n"
                                               "    f(): unit\n"
                                               "        a\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_interface, source_location_create(0, 5))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("impl string-ref for unit\n"
                                               "    f(): unit\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(0, 5))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("impl 1 for unit\n"
                                               "    f(): unit\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(2, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "impl i for 1\n"
                                               "    f(): unit\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_impl, source_location_create(5, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "impl i for unit\n"
                                               "    f(): unit\n"
                                               "        unit_value\n"
                                               "impl i for unit\n"
                                               "    f(): unit\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 3);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_cannot_capture_runtime_variable, source_location_create(5, 23))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "let runtime-value = side-effect()\n"
                                               "impl i for unit\n"
                                               "    f(): unit\n"
                                               "        let captured = runtime-value\n"
                                               "        unit_value\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(3, 19))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(a: boolean): unit\n"
                                               "impl i for unit\n"
                                               "    f(a: boolean): string-ref\n"
                                               "        \"\"\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(4, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "impl i for unit\n"
                                               "    f(): unit\n"
                                               "        \"\"\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_method_name, source_location_create(2, 4))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = interface\n"
                                               "    f(): unit\n"
                                               "    f(): unit\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_not_callable, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = {}()\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v = s{}\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(4, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = ()\n"
                                               "    side-effect()\n"
                                               "    1\n"
                                               "let s = f()\n"
                                               "let v = s{}\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(1, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = 1\n"
                                               "let v = s{}\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_structure, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v = unit{}\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_argument, source_location_create(2, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = struct\n"
                                               "    a: boolean\n"
                                               "let v = s{}\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_extraneous_argument, source_location_create(2, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = struct\n"
                                               "    a: boolean\n"
                                               "let v = s{boolean.true, boolean.false}\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 10))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = struct\n"
                                               "    a: boolean\n"
                                               "let v = s{u}\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = struct\n"
                                               "    a: unit\n"
                                               "    b: u\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(2, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let s = struct\n"
                                               "    a: unit\n"
                                               "    b: 1\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_expected_compile_time_type, source_location_create(6, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = ()\n"
                                               "    side-effect()\n"
                                               "    1\n"
                                               "let t = f()\n"
                                               "let s = struct\n"
                                               "    a: unit\n"
                                               "    b: t\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_match_case, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = match 0\n"
                                               "    case 0: 0\n"
                                               "    case 1: 1\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(1, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = match 0\n"
                                               "    case u: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(1, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let i = match 0\n"
                                               "    case boolean.true: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_match_case, source_location_create(3, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let input : int(0, 1) = 0\n"
                                               "let i = match input\n"
                                               "    case 0: 0\n"
                                               "    case 0: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_duplicate_match_case, source_location_create(4, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "match option.none\n"
                                               "    case option.some(let i): 0\n"
                                               "    case option.some(let i): 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_declaration_with_existing_name, source_location_create(4, 25))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "let i = 0\n"
                                               "match option.none\n"
                                               "    case option.some(let i): 0\n"
                                               "    case option.none: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(4, 21))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "let i = 0\n"
                                               "match option.none\n"
                                               "    case option.some(u): 0\n"
                                               "    case option.none: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(4, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "let i = 0\n"
                                               "match option.none\n"
                                               "    case u(let s): 0\n"
                                               "    case option.none: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_not_callable, source_location_create(4, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "let i = 0\n"
                                               "match option.none\n"
                                               "    case i(let s): 0\n"
                                               "    case option.none: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(2, 12))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let input : int(0, 1) = 0\n"
                                               "let i = match input\n"
                                               "    case 0: u\n"
                                               "    case 1: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(3, 12))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let input : int(0, 1) = 0\n"
                                               "let i = match input\n"
                                               "    case 0: 0\n"
                                               "    case 1: boolean.true\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(3, 9))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let std = import std\n"
                                               "let option = std.option\n"
                                               "match boolean.true\n"
                                               "    case option.some(let i): 0\n"
                                               "    case option.none: 0\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(1, 11))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let f = (): unit\n"
                                               "    return 2",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 2);
        checked_program_free(&checked);
    }

    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_break_outside_of_loop, source_location_create(0, 0))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("break\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 1);
        checked_program_free(&checked);
    }

    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_unknown_element, source_location_create(0, 15))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(boolean.something)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_no_members_on_enum_elements, source_location_create(0, 20))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(boolean.true.true)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(\"true\")", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("assert(2)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("assert(assert)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(boolean)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(side-effect())", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_missing_argument, source_location_create(0, 7))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("assert()", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_extraneous_argument, source_location_create(0, 21))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("assert(boolean.true, boolean.false)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        register_id *const arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = 2;
        instruction const expected_body_elements[] = {
            instruction_create_global(0), instruction_create_read_struct(read_struct_instruction_create(0, 4, 1)),
            instruction_create_literal(literal_instruction_create(
                2, value_from_enum_element(1, type_from_unit(), NULL), type_from_enumeration(0))),
            instruction_create_call(call_instruction_create(1, arguments, 1, 3)),
            instruction_create_return(return_instruction_create(3, 4))};
        instruction_sequence const expected_body = instruction_sequence_create(LPG_COPY_ARRAY(expected_body_elements));
        REQUIRE(instruction_sequence_equals(&expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_declaration_with_existing_name, source_location_create(1, 4))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("let v : boolean = boolean.true\n"
                                               "let v : boolean = boolean.true\n",
                                               std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 21))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked =
            simple_check("let v : int(1, 10) = 11\n", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }
    {
        semantic_error const errors[] = {
            semantic_error_create(semantic_error_type_mismatch, source_location_create(0, 8))};
        expected_errors expected = make_expected_errors(errors, LPG_ARRAY_SIZE(errors));
        checked_program checked = simple_check("assert(!3)", std_library.globals, &expected, module_directory_view);
        REQUIRE(expected.count == 0);
        checked_program_free(&checked);
    }
    unicode_string_free(&module_directory);
    standard_library_description_free(&std_library);
}
