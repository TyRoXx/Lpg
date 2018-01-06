#include "test_interpreter.h"
#include "test.h"
#include "lpg_interpret.h"
#include "lpg_stream_writer.h"
#include "handle_parse_error.h"
#include "lpg_check.h"
#include "lpg_standard_library.h"
#include "lpg_write_file.h"
#include <string.h>
#include "duktape.h"
#include "lpg_javascript_backend.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_read_file.h"
#include "path.h"
#include "lpg_c_backend.h"
#include "lpg_remove_dead_code.h"
#include "lpg_remove_unused_functions.h"
#include "lpg_save_expression.h"

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

static value assert_impl(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)captures;
    (void)environment;
    ASSUME(arguments.arguments[0].kind == value_kind_enum_element);
    enum_element_id const argument = arguments.arguments[0].enum_element.which;
    REQUIRE(argument == 1);
    return value_from_unit();
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

static void write_file_if_necessary(unicode_view const path, unicode_view const content)
{
    unicode_string const source_file_zero_terminated = unicode_view_zero_terminate(path);
    blob_or_error const existing = read_file(source_file_zero_terminated.data);
    if ((existing.error != NULL) ||
        !unicode_view_equals(unicode_view_create(existing.success.data, existing.success.length), content))
    {
        REQUIRE(success == write_file(path, content));
    }
    blob_free(&existing.success);
    unicode_string_free(&source_file_zero_terminated);
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
        write_file_if_necessary(unicode_view_from_string(source_file), c_source);
        unicode_string_free(&source_file);
    }
    {
        unicode_view const cmakelists_pieces[] = {
            unicode_view_from_string(c_test_dir), unicode_view_from_c_str("CMakeLists.txt")};
        unicode_string const cmakelists = path_combine(cmakelists_pieces, LPG_ARRAY_SIZE(cmakelists_pieces));
        memory_writer cmakelists_content = {NULL, 0, 0};
        stream_writer writer = memory_writer_erase(&cmakelists_content);
        REQUIRE(success == stream_writer_write_string(writer, "cmake_minimum_required(VERSION 3.2)\n"
                                                              "project(generated_test_solution)\n"
                                                              "if(MSVC)\n"
                                                              "    add_definitions(/WX)\n"
                                                              "    add_definitions(/wd4101)\n"
                                                              "endif()\n"));
        REQUIRE(success == stream_writer_write_string(writer, "include_directories(\""));
        {
            unicode_view const pieces[] = {path_remove_leaf(path_remove_leaf(unicode_view_from_c_str(__FILE__))),
                                           unicode_view_from_c_str("c_backend"),
                                           unicode_view_from_c_str("environment")};
            unicode_string const include_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
            REQUIRE(success == stream_writer_write_unicode_view(writer, unicode_view_from_string(include_path)));
            unicode_string_free(&include_path);
        }
        REQUIRE(success == stream_writer_write_string(writer, "\")\n"));
        REQUIRE(success == stream_writer_write_string(writer, "add_executable(generated_test generated_test.c)\n"));
        write_file_if_necessary(unicode_view_from_string(cmakelists),
                                unicode_view_create(cmakelists_content.data, cmakelists_content.used));
        memory_writer_free(&cmakelists_content);
        unicode_string_free(&cmakelists);
    }

    {
        unicode_view const cmakecache_pieces[] = {
            unicode_view_from_string(c_test_dir), unicode_view_from_c_str("CMakeCache.txt")};
        unicode_string const cmakecache = path_combine(cmakecache_pieces, LPG_ARRAY_SIZE(cmakecache_pieces));
        if (!file_exists(unicode_view_from_string(cmakecache)))
        {
            unicode_view const cmake_arguments[] = {
                unicode_view_from_c_str("-DCMAKE_BUILD_TYPE=DEBUG"), unicode_view_from_c_str(".")};
            create_process_result const cmake_process =
                create_process(unicode_view_from_c_str(LPG_CMAKE_EXECUTABLE), cmake_arguments,
                               LPG_ARRAY_SIZE(cmake_arguments), unicode_view_from_string(c_test_dir),
                               get_standard_input(), get_standard_output(), get_standard_error());
            REQUIRE(cmake_process.success == success);
            REQUIRE(0 == wait_for_process_exit(cmake_process.created));
        }
        unicode_string_free(&cmakecache);
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
#ifdef __linux__
        unicode_view const arguments[] = {unicode_view_from_c_str("--error-exitcode=42"),
                                          unicode_view_from_c_str("--leak-check=full"),
                                          unicode_view_from_string(test_executable)};
        create_process_result const process = create_process(
            unicode_view_from_c_str("/usr/bin/valgrind"), arguments, LPG_ARRAY_SIZE(arguments),
            unicode_view_from_string(c_test_dir), get_standard_input(), get_standard_output(), get_standard_error());
#else
        create_process_result const process =
            create_process(unicode_view_from_string(test_executable), NULL, 0, unicode_view_from_string(c_test_dir),
                           get_standard_input(), get_standard_output(), get_standard_error());
#endif
        unicode_string_free(&test_executable);
        REQUIRE(process.success == success);
        REQUIRE(0 == wait_for_process_exit(process.created));
    }
    unicode_string_free(&executable_path);
    unicode_string_free(&c_test_dir);
}

static void test_all_backends(unicode_view const test_name, checked_program const program,
                              structure const global_object)
{
    {
        value const globals_values[] = {
            /*type*/ value_from_unit(),
            /*string-ref*/ value_from_unit(),
            /*removed2*/ value_from_unit(),
            /*boolean*/ global_object.members[3].compile_time_value.value_,
            /*assert*/ value_from_function_pointer(function_pointer_value_from_external(assert_impl, NULL, NULL, 0)),
            /*and*/ value_from_function_pointer(function_pointer_value_from_external(and_impl, NULL, NULL, 0)),
            /*or*/ value_from_function_pointer(function_pointer_value_from_external(or_impl, NULL, NULL, 0)),
            /*not*/ value_from_function_pointer(function_pointer_value_from_external(not_impl, NULL, NULL, 0)),
            /*concat*/ value_from_function_pointer(function_pointer_value_from_external(concat_impl, NULL, NULL, 0)),
            /*string-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(string_equals_impl, NULL, NULL, 0)),
            /*removed*/ value_from_unit(),
            /*int*/ value_from_function_pointer(function_pointer_value_from_external(int_impl, NULL, NULL, 0)),
            /*integer-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_equals_impl, NULL, NULL, 0)),
            /*unit*/ value_from_unit(),
            /*unit_value*/ value_from_unit(),
            /*option*/ value_from_unit(),
            /*integer-less*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_less_impl, NULL, NULL, 0)),
            /*integer-to-string*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_to_string_impl, NULL, NULL, 0)),
            /*side-effect*/ value_from_function_pointer(
                function_pointer_value_from_external(side_effect_impl, NULL, NULL, 0))};
        LPG_STATIC_ASSERT(LPG_ARRAY_SIZE(globals_values) == standard_library_element_count);
        garbage_collector gc = {NULL};
        interpret(program, globals_values, &gc);
        garbage_collector_free(gc);
    }

    {
        memory_writer generated = {NULL, 0, 0};
        REQUIRE(success == generate_javascript(program, memory_writer_erase(&generated)));
        duk_context *const duktape =
            duk_create_heap(duktape_allocate, duktape_realloc, duktape_free, NULL, duktake_handle_fatal);
        REQUIRE(duktape);

        duk_push_c_function(duktape, javascript_assert, 1);
        duk_put_global_string(duktape, "assert");

        duk_eval_lstring(duktape, generated.data, generated.used);
        memory_writer_free(&generated);
        duk_destroy_heap(duktape);
    }

    {
        memory_writer generated = {NULL, 0, 0};
        REQUIRE(success == generate_c(program, memory_writer_erase(&generated)));
        run_c_test(test_name, unicode_view_create(generated.data, generated.used));
        memory_writer_free(&generated);
    }
}

static void expect_output_impl(unicode_view const test_name, unicode_view const source, structure const global_object)
{
    sequence const root = parse(source);

    {
        memory_writer buffer = {NULL, 0, 0};
        whitespace_state const whitespace = {0, false};
        REQUIRE(success == save_sequence(memory_writer_erase(&buffer), root, whitespace));
        sequence const reparsed = parse(unicode_view_create(buffer.data, buffer.used));
        sequence_free(&reparsed);
        memory_writer_free(&buffer);
    }

    checked_program const checked = check(root, global_object, expect_no_errors, NULL);
    sequence_free(&root);
    test_all_backends(test_name, checked, global_object);
    {
        checked_program optimized = remove_unused_functions(checked);
        remove_dead_code(&optimized);
        memory_writer optimized_test_name = {NULL, 0, 0};
        {
            stream_writer const writer = memory_writer_erase(&optimized_test_name);
            REQUIRE(success == stream_writer_write_unicode_view(writer, test_name));
            REQUIRE(success == stream_writer_write_string(writer, "+optimized"));
        }
        test_all_backends(
            unicode_view_create(optimized_test_name.data, optimized_test_name.used), optimized, global_object);
        memory_writer_free(&optimized_test_name);
        checked_program_free(&optimized);
    }
    checked_program_free(&checked);
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
    expect_output_impl(unicode_view_from_c_str(source_file), unicode_view_from_string(expected), global_object);
    unicode_string_free(&expected);
}

void test_interpreter(void)
{
    standard_library_description const std_library = describe_standard_library();

    {
        char const *const test_files[] = {"enum-stateful.lpg",
                                          "boolean.lpg",
                                          "concat.lpg",
                                          "empty.lpg",
                                          "enum-stateless.lpg",
                                          "function-pointer.lpg",
                                          "integer-equals.lpg",
                                          "integer-less.lpg",
                                          "integer-to-string.lpg",
                                          "interface.lpg",
                                          "lambda-capture.lpg",
                                          "lambda-parameters.lpg",
                                          "lambda-recapture.lpg",
                                          "lambda-return-type.lpg",
                                          "loop.lpg",
                                          "match-enum-stateful.lpg",
                                          "match-enum-stateless.lpg",
                                          "match-integer.lpg",
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

    standard_library_description_free(&std_library);
}
