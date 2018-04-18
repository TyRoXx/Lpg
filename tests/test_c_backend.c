#include "test_c_backend.h"
#include "test.h"
#include "lpg_check.h"
#include "handle_parse_error.h"
#include <string.h>
#include "lpg_standard_library.h"
#include "lpg_c_backend.h"
#include "lpg_read_file.h"
#include "lpg_allocate.h"
#include <stdio.h>
#include "lpg_remove_unused_functions.h"
#include "lpg_remove_dead_code.h"
#include "path.h"

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

static import_result failing_importer(unicode_view name, void *user)
{
    (void)name;
    (void)user;
    import_result const failure = {optional_value_empty, type_from_unit()};
    return failure;
}

static void check_generated_c_code(char const *const source, standard_library_description const standard_library)
{
    sequence root = parse(unicode_view_from_c_str(source));
    checked_program checked = check(root, standard_library.globals, expect_no_errors, failing_importer, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count >= 1);
    remove_dead_code(&checked);
    checked_program optimized = remove_unused_functions(checked);
    checked_program_free(&checked);

    memory_writer generated = {NULL, 0, 0};
    REQUIRE(success == generate_c(optimized, memory_writer_erase(&generated)));

    checked_program_free(&optimized);
    memory_writer_free(&generated);
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
                           "    side-effect()\n"
                           "    \"\"\n"
                           "assert(string-equals(read(), \"\"))\n",
                           std_library);

    check_generated_c_code("let f = () assert(boolean.true)\n"
                           "f()\n",
                           std_library);

    check_generated_c_code("let read = ()\n"
                           "    side-effect()\n"
                           "    \"\"\n"
                           "let s = (a: string-ref) a\n"
                           "let t = s(concat(read(), \"\"))\n"
                           "assert(string-equals(\"\", t))\n",
                           std_library);

    check_generated_c_code("let read = ()\n"
                           "    side-effect()\n"
                           "    \"b\"\n"
                           "assert(match string-equals(read(), \"a\")\n"
                           "    case boolean.false: boolean.true\n"
                           "    case boolean.true: boolean.false\n"
                           ")\n",
                           std_library);

    check_generated_c_code("let printable = interface\n"
                           "    print(): string-ref\n"
                           "impl printable for string-ref\n"
                           "    print()\n"
                           "        side-effect()\n"
                           "        self\n"
                           "let f = (printed: printable)\n"
                           "    side-effect()\n"
                           "    printed.print()\n"
                           "assert(string-equals(\"a\", f(\"a\")))\n"
                           "assert(string-equals(\"a\", f(f(\"a\"))))\n",
                           std_library);

    check_generated_c_code("let f = ()\n"
                           "    side-effect()\n"
                           "    123\n"
                           "assert(integer-less(122, f()))\n",
                           std_library);

    check_generated_c_code("let f = ()\n"
                           "    side-effect()\n"
                           "    123\n"
                           "assert(string-equals(\"123\", integer-to-string(f())))\n",
                           std_library);

    standard_library_description_free(&std_library);
}
