#include "test_interpreter.h"
#include "test.h"
#include "lpg_interpret.h"
#include "lpg_stream_writer.h"
#include "handle_parse_error.h"
#include "lpg_check.h"
#include "lpg_standard_library.h"
#include <string.h>
#include "duktape.h"
#include "lpg_javascript_backend.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_read_file.h"
#include "path.h"
#include "lpg_c_backend.h"

static sequence parse(unicode_view const input)
{
    test_parser_user user = {{input.begin, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(find_next_token, handle_error, &user);
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

typedef struct test_environment
{
    stream_writer print_destination;
} test_environment;

static value print(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)captures;
    unicode_view const text = arguments.arguments[0].string_ref;
    test_environment *const actual_environment = environment;
    stream_writer *destination = &actual_environment->print_destination;
    REQUIRE(stream_writer_write_bytes(*destination, text.begin, text.length) == success);
    return value_from_unit();
}

static value assert_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)captures;
    (void)environment;
    enum_element_id const argument = arguments.arguments[0].enum_element.which;
    REQUIRE(argument == 1);
    return value_from_unit();
}

static duk_ret_t javascript_print(duk_context *const duktape)
{
    duk_push_current_function(duktape);
    duk_get_prop_string(duktape, -1, DUK_HIDDEN_SYMBOL("print-buffer"));
    memory_writer *const print_buffer = duk_get_pointer(duktape, -1);
    REQUIRE(success == stream_writer_write_string(memory_writer_erase(print_buffer), duk_get_string(duktape, 0)));
    return 0;
}

static bool from_javascript_bool(double const value)
{
    if (value == 0.0)
    {
        return false;
    }
    if (value == 1.0)
    {
        return true;
    }
    FAIL();
}

static duk_ret_t javascript_assert(duk_context *const duktape)
{
    REQUIRE(from_javascript_bool(duk_get_number(duktape, 0)));
    return 0;
}

static void *duktape_allocate(void *udata, duk_size_t size)
{
    (void)udata;
    return allocate(size);
}

static void *duktape_realloc(void *udata, void *ptr, duk_size_t size)
{
    (void)udata;
    if (size == 0)
    {
        if (ptr)
        {
            deallocate(ptr);
        }
        return NULL;
    }
    return reallocate(ptr, size);
}

static void duktape_free(void *udata, void *ptr)
{
    (void)udata;
    if (!ptr)
    {
        return;
    }
    deallocate(ptr);
}

static void duktake_handle_fatal(void *udata, const char *msg)
{
    (void)udata;
    fprintf(stderr, "%s\n", msg);
    FAIL();
}

static success_indicator write_file(unicode_view const path, unicode_view const content)
{
    unicode_string const path_zero_terminated = unicode_view_zero_terminate(path);
    FILE *file;
#ifdef _MSC_VER
    fopen_s(&file, path_zero_terminated.data, "wb");
#else
    file = fopen(path_zero_terminated.data, "wb");
#endif
    unicode_string_free(&path_zero_terminated);
    if (!file)
    {
        return failure;
    }
    if (fwrite(content.begin, 1, content.length, file) != content.length)
    {
        fclose(file);
        return failure;
    }
    fclose(file);
    return success;
}

static void run_c_test(unicode_view const test_name, unicode_view const c_source)
{
    ASSUME(test_name.length > 0);
    unicode_string const executable_path = get_current_executable_path();
    unicode_view const executable_dir = path_remove_leaf(unicode_view_from_string(executable_path));
    unicode_view tests_dir = executable_dir;
#ifdef _MSC_VER
    tests_dir = path_remove_leaf(tests_dir);
#endif
    unicode_view const c_test_dir_pieces[] = {tests_dir, unicode_view_from_c_str("in_lpg"), test_name};
    unicode_string const c_test_dir = path_combine(c_test_dir_pieces, LPG_ARRAY_SIZE(c_test_dir_pieces));
    REQUIRE(success == create_directory(unicode_view_from_string(c_test_dir)));
    {
        unicode_view const source_pieces[] = {
            unicode_view_from_string(c_test_dir), unicode_view_from_c_str("generated_test.c")};
        unicode_string const source_file = path_combine(source_pieces, LPG_ARRAY_SIZE(source_pieces));
        REQUIRE(success == write_file(unicode_view_from_string(source_file), c_source));
        unicode_string_free(&source_file);
    }
    {
        unicode_view const cmakelists_pieces[] = {
            unicode_view_from_string(c_test_dir), unicode_view_from_c_str("CMakeLists.txt")};
        unicode_string const cmakelists = path_combine(cmakelists_pieces, LPG_ARRAY_SIZE(cmakelists_pieces));
        memory_writer cmakelists_content = {NULL, 0, 0};
        stream_writer writer = memory_writer_erase(&cmakelists_content);
        REQUIRE(success == stream_writer_write_string(writer, "cmake_minimum_required(VERSION 3.2)\n"
                                                              "project(generated_test_solution)\n"));
        REQUIRE(success == stream_writer_write_string(writer, "include_directories("));
        {
            unicode_view const pieces[] = {path_remove_leaf(path_remove_leaf(unicode_view_from_c_str(__FILE__))),
                                           unicode_view_from_c_str("c_backend"),
                                           unicode_view_from_c_str("environment")};
            unicode_string const include_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
            REQUIRE(success == stream_writer_write_unicode_view(writer, unicode_view_from_string(include_path)));
            unicode_string_free(&include_path);
        }
        REQUIRE(success == stream_writer_write_string(writer, ")\n"));
        REQUIRE(success == stream_writer_write_string(writer, "add_executable(generated_test generated_test.c)\n"));
        REQUIRE(success == write_file(unicode_view_from_string(cmakelists),
                                      unicode_view_create(cmakelists_content.data, cmakelists_content.used)));
        memory_writer_free(&cmakelists_content);
        unicode_string_free(&cmakelists);
    }
    {
        unicode_view const cmake_arguments[] = {unicode_view_from_c_str(".")};
        create_process_result const cmake_process = create_process(
            unicode_view_from_c_str(LPG_CMAKE_EXECUTABLE), cmake_arguments, LPG_ARRAY_SIZE(cmake_arguments),
            unicode_view_from_string(c_test_dir), get_standard_input(), get_standard_output(), get_standard_error());
        REQUIRE(cmake_process.success == success);
        REQUIRE(0 == wait_for_process_exit(cmake_process.created));
    }
    {
        unicode_view const cmake_arguments[] = {unicode_view_from_c_str("--build"), unicode_view_from_c_str(".")};
        create_process_result const cmake_process = create_process(
            unicode_view_from_c_str(LPG_CMAKE_EXECUTABLE), cmake_arguments, LPG_ARRAY_SIZE(cmake_arguments),
            unicode_view_from_string(c_test_dir), get_standard_input(), get_standard_output(), get_standard_error());
        REQUIRE(cmake_process.success == success);
        REQUIRE(0 == wait_for_process_exit(cmake_process.created));
    }
    {
        unicode_view const pieces[] = {unicode_view_from_string(c_test_dir),
#ifdef _MSC_VER
                                       unicode_view_from_c_str("Debug"),
#endif
                                       unicode_view_from_c_str("generated_test"
#ifdef _WIN32
                                                               ".exe"
#endif
                                                               )};
        unicode_string const test_executable = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
        create_process_result const process =
            create_process(unicode_view_from_string(test_executable), NULL, 0, unicode_view_from_string(c_test_dir),
                           get_standard_input(), get_standard_output(), get_standard_error());
        unicode_string_free(&test_executable);
        REQUIRE(process.success == success);
        REQUIRE(0 == wait_for_process_exit(process.created));
    }
    unicode_string_free(&executable_path);
    unicode_string_free(&c_test_dir);
}

static void expect_output_impl(unicode_view const test_name, unicode_view const source, char const *output,
                               structure const global_object)
{
    sequence const root = parse(source);
    checked_program const checked = check(root, global_object, expect_no_errors, NULL);

    {
        memory_writer print_buffer = {NULL, 0, 0};
        test_environment environment = {memory_writer_erase(&print_buffer)};
        value const globals_values[] = {
            /*type*/ value_from_unit(),
            /*string-ref*/ value_from_unit(),
            /*print*/ value_from_function_pointer(function_pointer_value_from_external(print, &environment, NULL, 0)),
            /*boolean*/ global_object.members[3].compile_time_value.value_,
            /*assert*/ value_from_function_pointer(function_pointer_value_from_external(assert_impl, NULL, NULL, 0)),
            /*and*/ value_from_function_pointer(function_pointer_value_from_external(and_impl, NULL, NULL, 0)),
            /*or*/ value_from_function_pointer(function_pointer_value_from_external(or_impl, NULL, NULL, 0)),
            /*not*/ value_from_function_pointer(function_pointer_value_from_external(not_impl, NULL, NULL, 0)),
            /*concat*/ value_from_function_pointer(function_pointer_value_from_external(concat_impl, NULL, NULL, 0)),
            /*string-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(string_equals_impl, NULL, NULL, 0)),
            /*removed*/ value_from_unit(),
            /*int*/ value_from_function_pointer(function_pointer_value_from_external(int_impl, &environment, NULL, 0)),
            /*integer-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_equals_impl, &environment, NULL, 0)),
            /*unit*/ value_from_unit(),
            /*unit_value*/ value_from_unit(),
            /*option*/ value_from_unit(),
            /*integer-less*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_less_impl, &environment, NULL, 0)),
            /*integer-to-string*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_to_string_impl, &environment, NULL, 0)),
            /*side-effect*/ value_from_function_pointer(
                function_pointer_value_from_external(side_effect_impl, &environment, NULL, 0))};
        LPG_STATIC_ASSERT(LPG_ARRAY_SIZE(globals_values) == standard_library_element_count);
        sequence_free(&root);
        garbage_collector gc = {NULL};
        interpret(checked, globals_values, &gc);
        garbage_collector_free(gc);

        REQUIRE(memory_writer_equals(print_buffer, output));
        memory_writer_free(&print_buffer);
    }

    {
        memory_writer print_buffer = {NULL, 0, 0};
        memory_writer generated = {NULL, 0, 0};
        REQUIRE(success == generate_javascript(checked, memory_writer_erase(&generated)));
        duk_context *const duktape =
            duk_create_heap(duktape_allocate, duktape_realloc, duktape_free, NULL, duktake_handle_fatal);
        REQUIRE(duktape);

        duk_push_c_function(duktape, javascript_print, 1);
        duk_push_pointer(duktape, &print_buffer);
        duk_put_prop_string(duktape, -2, DUK_HIDDEN_SYMBOL("print-buffer"));
        duk_put_global_string(duktape, "print");

        duk_push_c_function(duktape, javascript_assert, 1);
        duk_put_global_string(duktape, "assert");

        duk_eval_lstring(duktape, generated.data, generated.used);
        memory_writer_free(&generated);
        duk_destroy_heap(duktape);

        REQUIRE(memory_writer_equals(print_buffer, output));
        memory_writer_free(&print_buffer);
    }

    {
        memory_writer generated = {NULL, 0, 0};
        REQUIRE(success == generate_c(checked, global_object.members[3].compile_time_value.value_.type_.enum_,
                                      memory_writer_erase(&generated)));
        if (test_name.length > 0)
        {
            run_c_test(test_name, unicode_view_create(generated.data, generated.used));
        }
        memory_writer_free(&generated);
    }

    checked_program_free(&checked);
}

static void expect_output(char const *source, char const *output, structure const global_object)
{
    expect_output_impl(unicode_view_from_c_str(""), unicode_view_from_c_str(source), output, global_object);
}

static void run_file(char const *const source_file, structure const global_object)
{
    unicode_view const pieces[] = {path_remove_leaf(unicode_view_from_c_str(__FILE__)),
                                   unicode_view_from_c_str("in_lpg"), unicode_view_from_c_str(source_file)};
    unicode_string const full_expected_file_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    blob_or_error expected_or_error = read_file(full_expected_file_path.data);
    unicode_string_free(&full_expected_file_path);
    REQUIRE(!expected_or_error.error);
    unicode_string const expected = unicode_string_validate(expected_or_error.success);
    expect_output_impl(unicode_view_from_c_str(source_file), unicode_view_from_string(expected), "", global_object);
    unicode_string_free(&expected);
}

void test_interpreter(void)
{
    standard_library_description const std_library = describe_standard_library();

    {
        char const *const test_files[] = {"boolean.lpg",
                                          "concat.lpg",
                                          "empty.lpg",
                                          "integer-equals.lpg",
                                          "integer-less.lpg",
                                          "integer-to-string.lpg",
                                          "interface.lpg",
                                          "lambda-capture.lpg",
                                          "lambda-recapture.lpg",
                                          "lambda-return-type.lpg",
                                          "loop.lpg",
                                          "match-enum.lpg",
                                          "option.lpg",
                                          "raw-string-literal.lpg",
                                          "string-equals.lpg",
                                          "struct.lpg",
                                          "tuple.lpg",
                                          "unit_value.lpg"};
        for (size_t i = 0; i < LPG_ARRAY_SIZE(test_files); ++i)
        {
            run_file(test_files[i], std_library.globals);
        }
    }

    expect_output("let f = (a: string-ref, b: boolean)\n"
                  "    assert(string-equals(\"abc\", a))\n"
                  "    assert(not(b))\n"
                  "    print(a)\n"
                  "f(\"abc\", boolean.false)\n",
                  "abc", std_library.globals);

    standard_library_description_free(&std_library);
}
