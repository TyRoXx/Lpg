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

static void fix_line_endings(unicode_string *s)
{
    size_t const length = s->length;
    size_t last_written = 0;
    for (size_t i = 0; i < length; ++i)
    {
        if (s->data[i] == '\r')
        {
            continue;
        }
        s->data[last_written] = s->data[i];
        ++last_written;
    }
    s->length = last_written;
}

static void check_generated_c_code(char const *const source, standard_library_description const standard_library,
                                   char const *const expected_c_file_name)
{
    sequence root = parse(unicode_view_from_c_str(source));
    checked_program checked = check(root, standard_library.globals, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count >= 1);
    remove_dead_code(&checked);
    checked_program optimized = remove_unused_functions(checked);
    checked_program_free(&checked);

    memory_writer generated = {NULL, 0, 0};
    REQUIRE(success == generate_c(optimized, &standard_library.stable->boolean, memory_writer_erase(&generated)));
    unicode_view const pieces[] = {path_remove_leaf(unicode_view_from_c_str(__FILE__)),
                                   unicode_view_from_c_str("c_backend"), unicode_view_from_c_str(expected_c_file_name)};
    unicode_string const full_expected_file_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    blob_or_error expected_or_error = read_file(full_expected_file_path.data);
    REQUIRE(!expected_or_error.error);
    unicode_string expected = unicode_string_validate(expected_or_error.success);
    REQUIRE(expected.length == expected_or_error.success.length);
    fix_line_endings(&expected);
    {
        size_t const expected_total_length = expected.length;
        if (generated.used != expected_total_length)
        {
            fwrite(generated.data, 1, generated.used, stdout);
            FAIL();
        }
    }
    if (!unicode_view_equals(memory_writer_content(generated), unicode_view_from_string(expected)))
    {
        fwrite(generated.data, 1, generated.used, stdout);
        FAIL();
    }
    unicode_string_free(&full_expected_file_path);
    checked_program_free(&optimized);
    memory_writer_free(&generated);
    unicode_string_free(&expected);
}

void test_c_backend(void)
{
    standard_library_description const std_library = describe_standard_library();

    check_generated_c_code("", std_library, "0_empty.c");

    check_generated_c_code("print(\"Hello, world!\")\n", std_library, "1_hello_world.c");

    check_generated_c_code("print(\"Hello, \")\nprint(\"world!\\n\")\n", std_library, "2_print_twice.c");

    check_generated_c_code("loop\n"
                           "    print(\"Hello, world!\")\n"
                           "    break\n",
                           std_library, "3_loop_break.c");

    check_generated_c_code("let s = concat(\"123\", \"456\")\n"
                           "print(s)\n",
                           std_library, "4_concat_compile_time.c");

    check_generated_c_code("print(read())\n", std_library, "5_read.c");

    check_generated_c_code("loop\n"
                           "    break\n"
                           "    print(read())\n",
                           std_library, "6_loop_read.c");

    check_generated_c_code("loop\n"
                           "    break\n"
                           "    assert(boolean.false)\n",
                           std_library, "7_assert_false.c");

    check_generated_c_code("assert(boolean.true)\n", std_library, "8_assert_true.c");

    check_generated_c_code("assert(string-equals(read(), \"\"))\n", std_library, "9_string_equals.c");

    check_generated_c_code("print(concat(\"a\", read()))\n", std_library, "10_concat.c");

    check_generated_c_code("let f = ()\n"
                           "    print(\"\")\n"
                           "    boolean.true\n"
                           "assert(f())\n",
                           std_library, "11_lambda.c");

    check_generated_c_code("let f = () assert(boolean.true)\n"
                           "f()\n",
                           std_library, "12_lambda_call.c");

    check_generated_c_code("let id = (a: boolean)\n"
                           "    print(\"\")\n"
                           "    a\n"
                           "assert(id(boolean.true))\n",
                           std_library, "13_lambda_parameter.c");

    check_generated_c_code("let xor = (a: boolean, b: boolean)\n"
                           "    print(\"\")\n"
                           "    or(and(a, not(b)), and(not(a), b))\n"
                           "assert(xor(boolean.true, boolean.false))\n"
                           "assert(xor(boolean.false, boolean.true))\n"
                           "assert(not(xor(boolean.true, boolean.true)))\n"
                           "assert(not(xor(boolean.false, boolean.false)))\n",
                           std_library, "14_lambda_parameters.c");

    check_generated_c_code("let s = () read()\n"
                           "print(s())\n",
                           std_library, "15_return_string.c");

    check_generated_c_code("let s = (a: string-ref) print(a)\n"
                           "s(\"a\")\n",
                           std_library, "16_pass_string.c");

    check_generated_c_code("let s = (a: string-ref) print(a)\n"
                           "s(read())\n",
                           std_library, "17_pass_owning_string.c");

    check_generated_c_code("let s = (a: string-ref) a\n"
                           "let t = s(concat(read(), \"\"))\n"
                           "assert(string-equals(\"\", t))\n",
                           std_library, "18_move_string.c");

    check_generated_c_code("let s = (a: int(0, 3))\n"
                           "    print(\"\")\n"
                           "    a\n"
                           "let i = s(2)\n"
                           "assert(integer-equals(i, 2))\n",
                           std_library, "19_integer.c");

    check_generated_c_code("let s = ()\n"
                           "    print(\"\")\n"
                           "    (a: string-ref)\n"
                           "        print(\"\")\n"
                           "        a\n"
                           "let t = () (a: unit) \"a\"\n"
                           "let u = () (a: unit, b: unit) unit_value\n"
                           "let r = s()\n"
                           "assert(string-equals(r(\"a\"), \"a\"))\n",
                           std_library, "20_return_lambda.c");

    check_generated_c_code("assert(match string-equals(read(), \"a\")\n"
                           "    case boolean.false: boolean.true\n"
                           "    case boolean.true: boolean.false\n"
                           ")\n",
                           std_library, "21_match.c");

    check_generated_c_code("let f = ()\n"
                           "    print(\"\")\n"
                           "    boolean.true\n"
                           "assert({boolean.false, f(), {}}.1)\n",
                           std_library, "22_tuple.c");

    check_generated_c_code("let f = (a: boolean)\n"
                           "    print(\"\")\n"
                           "    ()\n"
                           "        print(\"\")\n"
                           "        a\n"
                           "assert(f(boolean.true)())\n",
                           std_library, "23_lambda_capture.c");

    check_generated_c_code("let outer = (a: string-ref)\n"
                           "    print(\"\")\n"
                           "    ()\n"
                           "        print(\"\")\n"
                           "        a\n"
                           "let inner = outer(\"a\")\n"
                           "let result = inner()\n"
                           "assert(string-equals(\"a\", result))\n",
                           std_library, "24_lambda_capture_owning.c");

    check_generated_c_code("let construct = ()\n"
                           "    print(\"\")\n"
                           "    let result = concat(read(), \"123\")\n"
                           "    {result, result}\n"
                           "let tuple = construct()\n"
                           "assert(string-equals(\"123\", tuple.0))\n",
                           std_library, "25_tuple_ownership.c");

    check_generated_c_code("let f = (printed: printable)\n"
                           "    let method = printed.print\n"
                           "    let string = method()\n"
                           "    print(string)\n",
                           std_library, "26_interface.c");

    standard_library_description_free(&std_library);
}
