#include "test_c_backend.h"
#include "find_builtin_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_c_backend.h"
#include "lpg_check.h"
#include "lpg_optimize.h"
#include "lpg_read_file.h"
#include "lpg_standard_library.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

static sequence parse(unicode_view const input, expression_pool *const pool)
{
    test_parser_user user = {{input.begin, input.length, source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(&user.base, handle_error, &user, pool);
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

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void check_generated_c_code(char const *const source, standard_library_description const standard_library)
{
    expression_pool pool = expression_pool_create();
    sequence root = parse(unicode_view_from_c_str(source), &pool);
    unicode_string const module_directory = find_builtin_module_directory();
    module_loader loader =
        module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
    source_file_lines_owning const lines = source_file_lines_owning_scan(unicode_view_from_c_str(source));
    checked_program checked =
        check(root, standard_library.globals, expect_no_errors, &loader,
              source_file_create(unicode_view_from_c_str("test.lpg"), unicode_view_from_c_str(source),
                                 source_file_lines_from_owning(lines)),
              unicode_view_from_string(module_directory), 100000, NULL);
    sequence_free(&root);
    source_file_lines_owning_free(lines);
    REQUIRE(checked.function_count >= 1);
    optimize(&checked);

    memory_writer generated = {NULL, 0, 0};
    {
        garbage_collector additional_memory = garbage_collector_create(SIZE_MAX);
        REQUIRE(success_yes == generate_c(checked, &additional_memory, memory_writer_erase(&generated)));
        garbage_collector_free(additional_memory);
    }
    checked_program_free(&checked);
    expression_pool_free(pool);
    memory_writer_free(&generated);
    unicode_string_free(&module_directory);
}

void test_c_backend(void)
{
    standard_library_description const std_library = describe_standard_library();

    check_generated_c_code("", std_library);

    check_generated_c_code("loop\n"
                           "    break\n"
                           "    assert(boolean.false)\n",
                           std_library);

    check_generated_c_code("assert(boolean.true)\n", std_library);

    check_generated_c_code("let read = ()\n"
                           "    side_effect()\n"
                           "    \"\"\n"
                           "assert(string_equals(read(), \"\"))\n",
                           std_library);

    check_generated_c_code("let f = () assert(boolean.true)\n"
                           "f()\n",
                           std_library);

    check_generated_c_code("let std = import std\n"
                           "let read = ()\n"
                           "    side_effect()\n"
                           "    \"\"\n"
                           "let s = (a: std.string) a\n"
                           "let t = s(concat(read(), \"\"))\n"
                           "assert(string_equals(\"\", t))\n",
                           std_library);

    check_generated_c_code("let read = ()\n"
                           "    side_effect()\n"
                           "    \"b\"\n"
                           "assert(match string_equals(read(), \"a\")\n"
                           "    case boolean.false: boolean.true\n"
                           "    case boolean.true: boolean.false\n"
                           ")\n",
                           std_library);

    check_generated_c_code("let std = import std\n"
                           "let printable = interface\n"
                           "    print(): std.string\n"
                           "impl printable for std.string\n"
                           "    print()\n"
                           "        side_effect()\n"
                           "        self\n"
                           "let f = (printed: printable)\n"
                           "    side_effect()\n"
                           "    printed.print()\n"
                           "assert(string_equals(\"a\", f(\"a\")))\n"
                           "assert(string_equals(\"a\", f(f(\"a\"))))\n",
                           std_library);

    check_generated_c_code("let f = ()\n"
                           "    side_effect()\n"
                           "    123\n"
                           "assert(integer_less(122, f()))\n",
                           std_library);

    check_generated_c_code("let f = ()\n"
                           "    side_effect()\n"
                           "    123\n"
                           "assert(string_equals(\"123\", integer_to_string(f())))\n",
                           std_library);

    standard_library_description_free(&std_library);
}
