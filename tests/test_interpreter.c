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

static sequence parse(char const *input)
{
    test_parser_user user = {{input, strlen(input), source_location_create(0, 0)}, NULL, 0};
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
    unicode_view read_input;
} test_environment;

static void test_captures(const standard_library_description *std_library);

static value print(value const *const inferred, value const *const arguments, garbage_collector *const gc,
                   void *environment)
{
    (void)inferred;
    (void)gc;
    unicode_view const text = arguments[0].string_ref;
    test_environment *const actual_environment = environment;
    stream_writer *destination = &actual_environment->print_destination;
    REQUIRE(stream_writer_write_bytes(*destination, text.begin, text.length) == success);
    return value_from_unit();
}

static value assert_impl(value const *const inferred, value const *arguments, garbage_collector *const gc,
                         void *environment)
{
    (void)environment;
    (void)inferred;
    (void)gc;
    enum_element_id const argument = arguments[0].enum_element.which;
    REQUIRE(argument == 1);
    return value_from_unit();
}

static value read_impl(value const *const inferred, value const *arguments, garbage_collector *const gc,
                       void *environment)
{
    (void)inferred;
    (void)gc;
    (void)arguments;
    test_environment *const actual_environment = environment;
    unicode_view const result = actual_environment->read_input;
    actual_environment->read_input = unicode_view_create(NULL, 0);
    return value_from_string_ref(result);
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

static duk_ret_t javascript_read(duk_context *const duktape)
{
    duk_push_current_function(duktape);
    duk_get_prop_string(duktape, -1, DUK_HIDDEN_SYMBOL("input"));
    unicode_view *const read_input = duk_get_pointer(duktape, -1);
    duk_push_lstring(duktape, read_input->begin, read_input->length);
    read_input->length = 0;
    return 1;
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

static void expect_output(char const *source, char const *input, char const *output, structure const global_object)
{
    sequence const root = parse(source);
    checked_program const checked = check(root, global_object, expect_no_errors, NULL);

    {
        memory_writer print_buffer = {NULL, 0, 0};
        test_environment environment = {memory_writer_erase(&print_buffer), unicode_view_from_c_str(input)};
        value const globals_values[] = {
            /*type*/ value_from_unit(),
            /*string-ref*/ value_from_unit(),
            /*print*/ value_from_function_pointer(function_pointer_value_from_external(print, &environment)),
            /*boolean*/ value_from_unit(),
            /*assert*/ value_from_function_pointer(function_pointer_value_from_external(assert_impl, NULL)),
            /*and*/ value_from_function_pointer(function_pointer_value_from_external(and_impl, NULL)),
            /*or*/ value_from_function_pointer(function_pointer_value_from_external(or_impl, NULL)),
            /*not*/ value_from_function_pointer(function_pointer_value_from_external(not_impl, NULL)),
            /*concat*/ value_from_function_pointer(function_pointer_value_from_external(concat_impl, NULL)),
            /*string-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(string_equals_impl, NULL)),
            /*read*/ value_from_function_pointer(function_pointer_value_from_external(read_impl, &environment)),
            /*int*/ value_from_function_pointer(function_pointer_value_from_external(int_impl, &environment)),
            /*integer-equals*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_equals_impl, &environment)),
            /*unit*/ value_from_unit(),
            /*unit_value*/ value_from_unit(),
            /*option*/ value_from_unit(),
            /*integer-less*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_less_impl, &environment)),
            /*integer-to-string*/ value_from_function_pointer(
                function_pointer_value_from_external(integer_to_string_impl, &environment))};
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

        unicode_view read_input = unicode_view_from_c_str(input);
        duk_push_c_function(duktape, javascript_read, 0);
        duk_push_pointer(duktape, &read_input);
        duk_put_prop_string(duktape, -2, DUK_HIDDEN_SYMBOL("input"));
        duk_put_global_string(duktape, "read");

        duk_eval_lstring(duktape, generated.data, generated.used);
        memory_writer_free(&generated);
        duk_destroy_heap(duktape);

        REQUIRE(memory_writer_equals(print_buffer, output));
        memory_writer_free(&print_buffer);
    }

    checked_program_free(&checked);
}

void test_interpreter(void)
{
    standard_library_description const std_library = describe_standard_library();

    expect_output("", "", "", std_library.globals);
    expect_output("print(\"\")", "", "", std_library.globals);
    expect_output("let a : int(0, 10) = 2\n"
                  "print(integer-to-string(a))",
                  "", "2", std_library.globals);

    expect_output("let a : int(0, 10) = 10\n"
                  "print(integer-to-string(a))",
                  "", "10", std_library.globals);
    expect_output("print(\"Hello, world!\")", "", "Hello, world!", std_library.globals);
    expect_output("print(\"Hello, world!\")\n", "", "Hello, world!", std_library.globals);
    expect_output("let v = \"Hello, world!\"\nprint(v)\n", "", "Hello, world!", std_library.globals);
    expect_output("print(\"Hello, world!\")\r\n", "", "Hello, world!", std_library.globals);
    expect_output("print(\"Hello, \")\nprint(\"world!\")", "", "Hello, world!", std_library.globals);
    expect_output("loop\n"
                  "    let v = \"Hello, world!\"\n"
                  "    print(v)\n"
                  "    break",
                  "", "Hello, world!", std_library.globals);
    expect_output("assert(!boolean.false)", "", "", std_library.globals);
    expect_output("assert(boolean.true)", "", "", std_library.globals);
    expect_output("assert(not(boolean.false))", "", "", std_library.globals);
    expect_output("assert(and(boolean.true, boolean.true))", "", "", std_library.globals);
    expect_output("assert(or(boolean.true, boolean.true))", "", "", std_library.globals);
    expect_output("assert(or(boolean.false, boolean.true))", "", "", std_library.globals);
    expect_output("assert(or(boolean.true, boolean.false))", "", "", std_library.globals);
    expect_output("let v = boolean.true\nassert(v)", "", "", std_library.globals);
    expect_output("let v = boolean.false\nassert(not(v))", "", "", std_library.globals);
    expect_output("let v = not(boolean.false)\nassert(v)", "", "", std_library.globals);
    expect_output("let v = 123\n", "", "", std_library.globals);
    expect_output("let s = concat(\"123\", \"456\")\nprint(s)\n", "", "123456", std_library.globals);
    expect_output("assert(string-equals(\"\", \"\"))\n", "", "", std_library.globals);
    expect_output("assert(string-equals(\"aaa\", \"aaa\"))\n", "", "", std_library.globals);
    expect_output("assert(string-equals(concat(\"aa\", \"a\"), \"aaa\"))\n", "", "", std_library.globals);
    expect_output("assert(string-equals(concat(\"aa\", read()), \"aaa\"))\n", "a", "", std_library.globals);
    expect_output("assert(not(string-equals(\"a\", \"\")))\n", "", "", std_library.globals);
    expect_output("assert(not(string-equals(\"a\", \"b\")))\n", "", "", std_library.globals);
    expect_output("assert(string-equals(read(), \"\"))\n", "", "", std_library.globals);
    expect_output("assert(string-equals(read(), \"aaa\"))\n", "aaa", "", std_library.globals);
    expect_output("let f = () boolean.true\n"
                  "assert(f())\n",
                  "", "", std_library.globals);
    expect_output("let f = () print(\"hello\")\n"
                  "f()\n",
                  "", "hello", std_library.globals);
    expect_output("let xor = (a: boolean, b: boolean)\n"
                  "    or(and(a, not(b)), and(not(a), b))\n"
                  "assert(xor(boolean.true, boolean.false))\n"
                  "assert(xor(boolean.false, boolean.true))\n"
                  "assert(not(xor(boolean.true, boolean.true)))\n"
                  "assert(not(xor(boolean.false, boolean.false)))\n",
                  "", "", std_library.globals);
    expect_output("let f = (arg: int(0, 1000))\n"
                  "    let force-runtime-evaluation = read()\n"
                  "    option.some(arg)\n"
                  "f(123)\n",
                  "", "", std_library.globals);
    expect_output("let s = {}\n"
                  "let t = {unit}\n"
                  "let u = {1, 2, 3, 4, 5, 6}\n"
                  "let v = {123, \"abc\"}\n"
                  "assert(integer-equals(123, v.0))\n"
                  "assert(string-equals(\"abc\", v.1))\n"
                  "let w = {{{{123}}}}\n"
                  "assert(integer-equals(123, w.0.0.0.0))\n",
                  "", "", std_library.globals);

    /*first case fits*/
    expect_output("assert(match boolean.true\n"
                  "    case boolean.true:\n"
                  "        print(\"jup\")\n"
                  "        boolean.true\n"
                  "    case boolean.false: boolean.false\n"
                  ")\n",
                  "", "jup", std_library.globals);

    /*second case fits*/
    expect_output("assert(match boolean.true\n"
                  "    case boolean.false: boolean.false\n"
                  "    case boolean.true:\n"
                  "        print(\"jup\")\n"
                  "        boolean.true\n"
                  ")\n",
                  "", "jup", std_library.globals);

    /* Integer less and greater than */
    expect_output("let small = 20\n"
                  "let big = 100\n"
                  "assert(integer-less(small, big))\n"
                  "assert(not(integer-less(big, small)))\n",
                  "", "", std_library.globals);

    test_captures(&std_library);

    standard_library_description_free(&std_library);
}

static void test_captures(const standard_library_description *std_library)
{
    expect_output("let m = \"hallo\"\n"
                  "let f = () print(m)\n"
                  "f()\n",
                  "", "hallo", (*std_library).globals);

    /*re-capture a captured constant*/
    expect_output("let m = \"y\"\n"
                  "let f = ()\n"
                  "    print(m)\n"
                  "    ()\n"
                  "        print(m)\n"
                  "f()()\n",
                  "", "yy", (*std_library).globals);

    /*re-capture a captured runtime variable*/
    expect_output("let m = read()\n"
                  "let f = ()\n"
                  "    print(m)\n"
                  "    ()\n"
                  "        print(m)\n"
                  "f()()\n",
                  "y", "yy", (*std_library).globals);

    /*capture multiple variables*/
    expect_output("let m = \"y\"\n"
                  "let f = ()\n"
                  "    let n = \"z\"\n"
                  "    print(m)\n"
                  "    print(n)\n"
                  "    ()\n"
                  "        print(n)\n"
                  "        print(m)\n"
                  "f()()\n",
                  "", "yzzy", (*std_library).globals);

    /*use a captured variable in a compile-time context*/
    expect_output("let m = boolean\n"
                  "let f = ()\n"
                  "    let a : m = boolean.true\n"
                  "    a\n"
                  "assert(f())\n",
                  "", "", (*std_library).globals);

    /*capture an argument*/
    expect_output("let f = (a: boolean)\n"
                  "    () a\n"
                  "assert(f(boolean.true)())\n",
                  "", "", (*std_library).globals);
}
