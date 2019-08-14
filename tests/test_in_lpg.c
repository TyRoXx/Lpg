#include "test_in_lpg.h"
#include "find_builtin_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_c_backend.h"
#include "lpg_check.h"
#include "lpg_copy_file.h"
#include "lpg_ecmascript_backend.h"
#include "lpg_interpret.h"
#include "lpg_load_module.h"
#include "lpg_monotonic_clock.h"
#include "lpg_optimize.h"
#include "lpg_path.h"
#include "lpg_read_file.h"
#include "lpg_remove_dead_code.h"
#include "lpg_rename_file.h"
#include "lpg_save_expression.h"
#include "lpg_standard_library.h"
#include "lpg_stream_writer.h"
#include "lpg_thread.h"
#include "lpg_write_file.h"
#include "lpg_write_file_if_necessary.h"
#include "test.h"
#include <inttypes.h>
#include <string.h>
#ifdef LPG_WITH_DUKTAPE
#include "duktape.h"
#endif

static sequence parse(unicode_view const input, expression_pool *const pool)
{
    test_parser_user user = {{input.begin, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(&user, handle_error, &user, pool);
    sequence const result = parse_program(&parser);
    expression_parser_free(parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(complete_semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static external_function_result assert_impl(value const *const captures, void *environment, optional_value const self,
                                            value *const arguments, interpreter *const context)
{
    (void)context;
    (void)self;
    (void)captures;
    (void)environment;
    ASSUME(arguments[0].kind == value_kind_enum_element);
    enum_element_id const argument = arguments[0].enum_element.which;
    REQUIRE(argument == 1);
    return external_function_result_from_success(value_from_unit());
}

#ifdef LPG_WITH_DUKTAPE
static duk_ret_t ecmascript_assert(duk_context *const duktape)
{
    duk_int_t const argument_type = duk_get_type(duktape, 0);
    duk_inspect_callstack_entry(duktape, -3);
    duk_get_prop_string(duktape, -1, "lineNumber");
    long const line = (long)duk_to_int(duktape, -1);
    duk_pop_2(duktape);
    REQUIRE_WITH_MESSAGE((argument_type == DUK_TYPE_BOOLEAN), "immediate caller is executing on line %ld\n", line);
    REQUIRE_WITH_MESSAGE(duk_get_boolean(duktape, 0), "immediate caller is executing on line %ld\n", line);
    return 0;
}

static duk_ret_t ecmascript_fail(duk_context *const duktape)
{
    duk_inspect_callstack_entry(duktape, -3);
    duk_get_prop_string(duktape, -1, "lineNumber");
    long const line = (long)duk_to_int(duktape, -1);
    duk_pop_2(duktape);
    fprintf(stderr, "failure on line %ld\n", line);
    FAIL();
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
    memory_writer const *const source = udata;
    fwrite(source->data, source->used, 1, stderr);
    fprintf(stderr, "\n%s\n", msg);
    FAIL();
}
#endif

static void run_c_test(unicode_view const test_name, unicode_view const c_source, unicode_view const c_test_dir)
{
    ASSUME(test_name.length > 0);
    unicode_view const last_success_pieces[] = {c_test_dir, unicode_view_from_c_str("last_success.c")};
    unicode_string const last_success = path_combine(last_success_pieces, LPG_ARRAY_SIZE(last_success_pieces));
    {
        unicode_string const last_success_zero = unicode_view_zero_terminate(unicode_view_from_string(last_success));
        blob_or_error const last_success_read = read_file(last_success_zero.data);
        unicode_string_free(&last_success_zero);
        if (!last_success_read.error)
        {
            bool const equal = ((last_success_read.success.length == c_source.length) &&
                                !memcmp(last_success_read.success.data, c_source.begin, c_source.length));
            blob_free(&last_success_read.success);
            if (equal)
            {
                goto out;
            }
        }
    }
    unicode_view const source_pieces[] = {c_test_dir, unicode_view_from_c_str("generated_test.c")};
    unicode_string const source_file_path = path_combine(source_pieces, LPG_ARRAY_SIZE(source_pieces));
    write_file_if_necessary(unicode_view_from_string(source_file_path), c_source);
    {
        unicode_view const cmakelists_pieces[] = {c_test_dir, unicode_view_from_c_str("CMakeLists.txt")};
        unicode_string const cmakelists = path_combine(cmakelists_pieces, LPG_ARRAY_SIZE(cmakelists_pieces));
        memory_writer cmakelists_content = {NULL, 0, 0};
        stream_writer writer = memory_writer_erase(&cmakelists_content);
        REQUIRE(success_yes == stream_writer_write_string(writer, "cmake_minimum_required(VERSION 3.2)\n"
                                                                  "project(generated_test_solution C)\n"
                                                                  "if(MSVC)\n"
                                                                  "    add_definitions(/WX)\n"
                                                                  "    add_definitions(/wd4101)\n"
                                                                  "    add_definitions(/wd4717)\n"
                                                                  "else()\n"
                                                                  "    add_definitions(-std=gnu99)\n"
                                                                  "endif()\n"));
        REQUIRE(success_yes == stream_writer_write_string(writer, "include_directories(\""));
        {
            unicode_view const pieces[] = {path_remove_leaf(path_remove_leaf(unicode_view_from_c_str(__FILE__))),
                                           unicode_view_from_c_str("c_backend"),
                                           unicode_view_from_c_str("environment")};
            unicode_string const include_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
            REQUIRE(success_yes == stream_writer_write_unicode_view(writer, unicode_view_from_string(include_path)));
            unicode_string_free(&include_path);
        }
        REQUIRE(success_yes == stream_writer_write_string(writer, "\")\n"));
        REQUIRE(success_yes == stream_writer_write_string(writer, "add_executable(generated_test generated_test.c)\n"));
        write_file_if_necessary(unicode_view_from_string(cmakelists),
                                unicode_view_create(cmakelists_content.data, cmakelists_content.used));
        memory_writer_free(&cmakelists_content);
        unicode_string_free(&cmakelists);
    }

    {
        unicode_view const cmakecache_pieces[] = {c_test_dir, unicode_view_from_c_str("CMakeCache.txt")};
        unicode_string const cmakecache = path_combine(cmakecache_pieces, LPG_ARRAY_SIZE(cmakecache_pieces));
        if (!file_exists(unicode_view_from_string(cmakecache)))
        {
            unicode_view const cmake_arguments[] = {unicode_view_from_c_str("-DCMAKE_BUILD_TYPE=DEBUG"),
#ifndef _MSC_VER
                                                    unicode_view_from_c_str("-G"),
                                                    unicode_view_from_c_str("CodeBlocks - Ninja"),
#endif
                                                    unicode_view_from_c_str(".")};
            create_process_result const cmake_process = create_process(
                unicode_view_from_c_str(LPG_CMAKE_EXECUTABLE), cmake_arguments, LPG_ARRAY_SIZE(cmake_arguments),
                c_test_dir, get_standard_input(), get_standard_output(), get_standard_error());
            REQUIRE(cmake_process.success == success_yes);
            REQUIRE(0 == wait_for_process_exit(cmake_process.created));
        }
        unicode_string_free(&cmakecache);
    }

    {
        unicode_view const cmake_arguments[] = {unicode_view_from_c_str("--build"), unicode_view_from_c_str(".")};
        create_process_result const cmake_process = create_process(
            unicode_view_from_c_str(LPG_CMAKE_EXECUTABLE), cmake_arguments, LPG_ARRAY_SIZE(cmake_arguments), c_test_dir,
            get_standard_input(), get_standard_output(), get_standard_error());
        REQUIRE(cmake_process.success == success_yes);
        int const exit_code = wait_for_process_exit(cmake_process.created);
        REQUIRE(0 == exit_code);
    }
    {
        unicode_view const pieces[] = {c_test_dir,
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
        create_process_result const process =
            create_process(unicode_view_from_c_str("valgrind"), arguments, LPG_ARRAY_SIZE(arguments), c_test_dir,
                           get_standard_input(), get_standard_output(), get_standard_error());
#else
        create_process_result const process =
            create_process(unicode_view_from_string(test_executable), NULL, 0, c_test_dir, get_standard_input(),
                           get_standard_output(), get_standard_error());
#endif
        unicode_string_free(&test_executable);
        REQUIRE(process.success == success_yes);
        REQUIRE(0 == wait_for_process_exit(process.created));
        REQUIRE(copy_file(unicode_view_from_string(source_file_path), unicode_view_from_string(last_success)));
    }
    unicode_string_free(&source_file_path);
out:
    unicode_string_free(&last_success);
}

static void test_all_backends(unicode_view const test_name, checked_program const program,
                              structure const global_object, unicode_view const c_test_dir)
{
    REQUIRE(success_yes == create_directory(c_test_dir));
    {
        value const globals_values[] = {
            /*0 side_effect*/ value_from_function_pointer(function_pointer_value_from_external(
                side_effect_impl, NULL, NULL, *global_object.members[0].what.function_pointer_)),
            /*1 integer_to_string*/ value_from_function_pointer(function_pointer_value_from_external(
                integer_to_string_impl, NULL, NULL, *global_object.members[1].what.function_pointer_)),
            /*2 type_equals*/ value_from_unit(),
            /*3 boolean*/ global_object.members[3].compile_time_value.value_,
            /*4 assert*/ value_from_function_pointer(function_pointer_value_from_external(
                assert_impl, NULL, NULL, *global_object.members[4].what.function_pointer_)),
            /*5 integer_less*/ value_from_function_pointer(function_pointer_value_from_external(
                integer_less_impl, NULL, NULL, *global_object.members[5].what.function_pointer_)),
            /*6 integer_equals*/ value_from_function_pointer(function_pointer_value_from_external(
                integer_equals_impl, NULL, NULL, *global_object.members[6].what.function_pointer_)),
            /*7 not*/ value_from_function_pointer(function_pointer_value_from_external(
                not_impl, NULL, NULL, *global_object.members[7].what.function_pointer_)),
            /*8 concat*/ value_from_function_pointer(function_pointer_value_from_external(
                concat_impl, NULL, NULL, *global_object.members[8].what.function_pointer_)),
            /*9 string_equals*/ value_from_function_pointer(function_pointer_value_from_external(
                string_equals_impl, NULL, NULL, *global_object.members[9].what.function_pointer_)),
            /*10 int*/ value_from_function_pointer(function_pointer_value_from_external(
                int_impl, NULL, NULL, *global_object.members[10].what.function_pointer_)),
            /*11 host_value*/ value_from_type(type_from_host_value()),
            /*12 fail*/ value_from_function_pointer(function_pointer_value_from_external(
                fail_impl, NULL, NULL, *global_object.members[12].what.function_pointer_)),
            /*13 subtract_result*/ global_object.members[13].compile_time_value.value_,
            /*14 subtract*/ global_object.members[14].compile_time_value.value_,
            /*15 add_result*/ global_object.members[15].compile_time_value.value_,
            /*16 add*/ global_object.members[16].compile_time_value.value_,
            /*17 add_u32_result*/ global_object.members[17].compile_time_value.value_,
            /*18 add_u32*/ global_object.members[18].compile_time_value.value_,
            /*19 add_u64_result*/ global_object.members[19].compile_time_value.value_,
            /*20 add_u64*/ global_object.members[20].compile_time_value.value_,
            /*21 and_u64*/ global_object.members[21].compile_time_value.value_,
            /*22 or_u64*/ global_object.members[22].compile_time_value.value_,
            /*23 xor_u64*/ global_object.members[23].compile_time_value.value_,
            /*24 not_u64*/ global_object.members[24].compile_time_value.value_,
            /*25 shift_left_u64*/ global_object.members[25].compile_time_value.value_,
            /*26 shift_right_u64*/ global_object.members[26].compile_time_value.value_
            //
        };
        LPG_STATIC_ASSERT(LPG_ARRAY_SIZE(globals_values) == standard_library_element_count);
        garbage_collector gc = garbage_collector_create(SIZE_MAX);
        interpret(program, globals_values, &gc);
        garbage_collector_free(gc);
    }

    {
        memory_writer generated = {NULL, 0, 0};
        bool const is_ecmascript_specific = unicode_view_equals_c_str(test_name, "ecmascript.lpg");
        if (is_ecmascript_specific)
        {
            REQUIRE(success_yes == stream_writer_write_string(memory_writer_erase(&generated), "var main = "));
        }
        enum_encoding_strategy_cache strategy_cache =
            enum_encoding_strategy_cache_create(program.enums, program.enum_count);
        unicode_string const builtins = load_ecmascript_builtins();
        REQUIRE(success_yes ==
                generate_ecmascript(program, &strategy_cache, unicode_view_from_string(builtins), &generated));
        unicode_string_free(&builtins);
        if (is_ecmascript_specific)
        {
            REQUIRE(success_yes == generate_host_class(&strategy_cache, get_host_interface(program), program.interfaces,
                                                       memory_writer_erase(&generated)));
            REQUIRE(success_yes == stream_writer_write_string(memory_writer_erase(&generated),
                                                              "main(new Function('return this;')(), new Host());\n"));
        }
        enum_encoding_strategy_cache_free(strategy_cache);
        {
            unicode_view const source_pieces[] = {c_test_dir, unicode_view_from_c_str("generated.js")};
            unicode_string const source_file_path = path_combine(source_pieces, LPG_ARRAY_SIZE(source_pieces));
            write_file_if_necessary(
                unicode_view_from_string(source_file_path), unicode_view_create(generated.data, generated.used));
            unicode_string_free(&source_file_path);
        }

#ifdef LPG_WITH_DUKTAPE
        {
            duk_context *const duktape =
                duk_create_heap(duktape_allocate, duktape_realloc, duktape_free, &generated, duktake_handle_fatal);
            REQUIRE(duktape);

            duk_push_c_function(duktape, ecmascript_assert, 1);
            duk_put_global_string(duktape, "builtin_assert");

            duk_push_c_function(duktape, ecmascript_fail, 1);
            duk_put_global_string(duktape, "builtin_fail");

            duk_eval_lstring(duktape, generated.data, generated.used);

            duk_destroy_heap(duktape);
        }
#endif

#if LPG_WITH_NODEJS
        {
            unicode_view const node = unicode_view_from_c_str(LPG_NODEJS);
            REQUIRE(success_yes == memory_writer_write(&generated, "", 1));
            unicode_string source_file_name = write_temporary_file(generated.data);
            unicode_view const argument = unicode_view_from_string(source_file_name);
            create_process_result const process_created = create_process(
                node, &argument, 1, c_test_dir, get_standard_input(), get_standard_output(), get_standard_error());
            REQUIRE(process_created.success == success_yes);
            int const exit_code = wait_for_process_exit(process_created.created);
            REQUIRE(0 == exit_code);
            REQUIRE(0 == remove(unicode_string_c_str(&source_file_name)));
            unicode_string_free(&source_file_name);
        }
#endif
        memory_writer_free(&generated);
    }

    {
        memory_writer generated = {NULL, 0, 0};
        {
            garbage_collector additional_memory = garbage_collector_create(SIZE_MAX);
            REQUIRE(success_yes == generate_c(program, &additional_memory, memory_writer_erase(&generated)));
            garbage_collector_free(additional_memory);
        }
        run_c_test(test_name, unicode_view_create(generated.data, generated.used), c_test_dir);
        memory_writer_free(&generated);
    }
}

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void expect_output_impl(unicode_view const test_name, unicode_view const source, structure const global_object,
                               unicode_view const in_lpg_directory, unicode_view const current_import_directory)
{
    expression_pool pool = expression_pool_create();
    sequence const root = parse(source, &pool);

    {
        memory_writer buffer = {NULL, 0, 0};
        whitespace_state const whitespace = {0, false};
        REQUIRE(success_yes == save_sequence(memory_writer_erase(&buffer), root, whitespace));
        sequence const reparsed = parse(unicode_view_create(buffer.data, buffer.used), &pool);
        sequence_free(&reparsed);
        memory_writer_free(&buffer);
    }

    unicode_string const module_directory = find_builtin_module_directory();
    module_loader loader =
        module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
    source_file_lines_owning const lines = source_file_lines_owning_scan(source);
    checked_program checked = check(root, global_object, expect_no_errors, &loader,
                                    source_file_create(test_name, source, source_file_lines_from_owning(lines)),
                                    current_import_directory, 200000, NULL);
    sequence_free(&root);
    source_file_lines_owning_free(lines);

    // not optimized
    {
        unicode_view const c_test_dir_pieces[] = {in_lpg_directory, test_name};
        unicode_string const c_test_dir = path_combine(c_test_dir_pieces, LPG_ARRAY_SIZE(c_test_dir_pieces));
        test_all_backends(test_name, checked, global_object, unicode_view_from_string(c_test_dir));
        unicode_string_free(&c_test_dir);
    }

    // fully optimized
    {
        optimize(&checked);
        memory_writer optimized_test_name = {NULL, 0, 0};
        {
            stream_writer const writer = memory_writer_erase(&optimized_test_name);
            REQUIRE(success_yes == stream_writer_write_unicode_view(writer, test_name));
            REQUIRE(success_yes == stream_writer_write_string(writer, "+optimized"));
        }
        unicode_view const optimized_test_name_view =
            unicode_view_create(optimized_test_name.data, optimized_test_name.used);
        unicode_view const c_test_dir_pieces[] = {in_lpg_directory, optimized_test_name_view};
        unicode_string const c_test_dir = path_combine(c_test_dir_pieces, LPG_ARRAY_SIZE(c_test_dir_pieces));
        test_all_backends(optimized_test_name_view, checked, global_object, unicode_view_from_string(c_test_dir));
        unicode_string_free(&c_test_dir);
        memory_writer_free(&optimized_test_name);
    }

    unicode_string_free(&module_directory);
    checked_program_free(&checked);
    expression_pool_free(pool);
}

static void run_file(char const *const source_file_name, unicode_view const in_lpg_subdirectory,
                     structure const global_object, unicode_view const in_lpg_directory)
{
    unicode_view const pieces[] = {path_remove_leaf(unicode_view_from_c_str(__FILE__)), in_lpg_subdirectory,
                                   unicode_view_from_c_str(source_file_name)};
    unicode_string const full_expected_file_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    blob_or_error const expected_or_error = read_file(full_expected_file_path.data);
    REQUIRE(!expected_or_error.error);
    unicode_string const expected = unicode_string_validate(expected_or_error.success);

    expect_output_impl(unicode_view_from_c_str(source_file_name), unicode_view_from_string(expected), global_object,
                       in_lpg_directory, path_remove_leaf(unicode_view_from_string(full_expected_file_path)));
    unicode_string_free(&expected);
    unicode_string_free(&full_expected_file_path);
}

typedef struct run_file_in_thread_state
{
    lpg_thread thread;
    char const *file;
    structure globals;
    unicode_view in_lpg_directory;
    duration how_long_did_it_take;
} run_file_in_thread_state;

static void run_file_in_thread(void *argument)
{
    run_file_in_thread_state *const cast = argument;
    duration const time_before = read_monotonic_clock();
    run_file(cast->file, unicode_view_from_c_str("in_lpg"), cast->globals, cast->in_lpg_directory);
    duration const time_after = read_monotonic_clock();
    duration const difference = absolute_duration_difference(time_before, time_after);
    cast->how_long_did_it_take = difference;
}

static int compare_threads_by_how_long_they_took(void const *const left, void const *const right)
{
    run_file_in_thread_state const *const real_left = left;
    run_file_in_thread_state const *const real_right = right;
    if (real_left->how_long_did_it_take.milliseconds > real_right->how_long_did_it_take.milliseconds)
    {
        return 1;
    }
    if (real_left->how_long_did_it_take.milliseconds < real_right->how_long_did_it_take.milliseconds)
    {
        return -1;
    }
    return 0;
}

static unicode_string find_tests_build_directory(void)
{
    unicode_string executable_path = get_current_executable_path();
    unicode_view executable_dir = path_remove_leaf(unicode_view_from_string(executable_path));
    unicode_view tests_dir = executable_dir;
#ifdef _MSC_VER
    tests_dir = path_remove_leaf(tests_dir);
#endif
    executable_path.length = tests_dir.length;
    return executable_path;
}

void test_in_lpg(void)
{
    standard_library_description const std_library = describe_standard_library();

    {
        unicode_string const tests_build_dir = find_tests_build_directory();
        unicode_view const in_lpg_dir_pieces[] = {
            unicode_view_from_string(tests_build_dir), unicode_view_from_c_str("in_lpg")};
        unicode_string const in_lpg_dir = path_combine(in_lpg_dir_pieces, LPG_ARRAY_SIZE(in_lpg_dir_pieces));
        unicode_string_free(&tests_build_dir);

        REQUIRE(create_directory(unicode_view_from_string(in_lpg_dir)) == success_yes);

        static char const *const test_files[] = {
            "c_ffi_hello.lpg", "comment_multi.lpg", "comment_single.lpg", "ecmascript.lpg", "empty.lpg", "web.lpg"};
        run_file_in_thread_state threads[LPG_ARRAY_SIZE(test_files)];
        size_t joined_until = (size_t)0 - 1;
        for (size_t i = 0; i < LPG_ARRAY_SIZE(threads); ++i)
        {
            run_file_in_thread_state *const state = threads + i;
            state->file = test_files[i];
            state->globals = std_library.globals;
            state->in_lpg_directory = unicode_view_from_string(in_lpg_dir);
            create_thread_result const created = create_thread(run_file_in_thread, state);
            REQUIRE(created.is_success == success_yes);
            state->thread = created.success;
            size_t const max_concurrency = 6;
            if ((i + 1) >= max_concurrency)
            {
                joined_until = (i + 1 - max_concurrency);
                join_thread(threads[joined_until].thread);
            }
        }
        for (size_t i = (joined_until + 1); i < LPG_ARRAY_SIZE(threads); ++i)
        {
            join_thread(threads[i].thread);
        }

        qsort(threads, LPG_ARRAY_SIZE(threads), sizeof(*threads), compare_threads_by_how_long_they_took);
        for (size_t i = 0; i < LPG_ARRAY_SIZE(threads); ++i)
        {
            run_file_in_thread_state *const thread = threads + i;
            printf("%" PRIu64 " ms %s\n", thread->how_long_did_it_take.milliseconds, thread->file);
        }

        unicode_string_free(&in_lpg_dir);
    }

    standard_library_description_free(&std_library);
}

void test_in_lpg_2(void)
{
    standard_library_description const std_library = describe_standard_library();
    unicode_string const tests_build_dir = find_tests_build_directory();
    unicode_view const in_lpg_2_dir_pieces[] = {
        unicode_view_from_string(tests_build_dir), unicode_view_from_c_str("in_lpg_2")};
    unicode_string const in_lpg_2_dir = path_combine(in_lpg_2_dir_pieces, LPG_ARRAY_SIZE(in_lpg_2_dir_pieces));
    unicode_string_free(&tests_build_dir);
    run_file("all_tests.lpg", unicode_view_from_c_str("in_lpg_2"), std_library.globals,
             unicode_view_from_string(in_lpg_2_dir));
    unicode_string_free(&in_lpg_2_dir);
    standard_library_description_free(&std_library);
}
