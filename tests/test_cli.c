#include "test_cli.h"
#include "find_builtin_module_directory.h"
#include "lpg_allocate.h"
#include "lpg_array_size.h"
#include "lpg_cli.h"
#include "lpg_read_file.h"
#include "lpg_write_file.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static void expect_output(int argc, char **argv, bool const expected_exit_code, char const *const expected_diagnostics,
                          unicode_view const current_directory)
{
    unicode_string const module_directory = find_builtin_module_directory();
    memory_writer diagnostics = {NULL, 0, 0};
    bool const real_error_code = run_cli(
        argc, argv, memory_writer_erase(&diagnostics), current_directory, unicode_view_from_string(module_directory));
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    REQUIRE(expected_exit_code == real_error_code);
    memory_writer_free(&diagnostics);
    unicode_string_free(&module_directory);
}

static void expect_output_with_source(char const *const source, bool const expected_exit_code,
                                      char const *const expected_diagnostics, unicode_view const current_directory)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[] = {"lpg", "run", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics, current_directory);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void expect_output_with_source_flags(char const *const source, char *const command,
                                            bool const expected_exit_code, char const *const expected_diagnostics,
                                            unicode_view const current_directory)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[3];
    arguments[0] = "lpg";
    arguments[1] = command;
    arguments[2] = unicode_string_c_str(&name);
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics, current_directory);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void formatting_tool(char const *const source, char const *const expected_output, bool const expected_exit_code,
                            char const *const expected_diagnostics, unicode_view const current_directory)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[] = {"lpg", "format", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics, current_directory);
    blob_or_error const read = read_file(unicode_string_c_str(&name));
    REQUIRE(!read.error);
    REQUIRE(unicode_view_equals_c_str(unicode_view_create(read.success.data, read.success.length), expected_output));
    blob_free(&read.success);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void test_formatting_tool(unicode_view const current_directory)
{
    formatting_tool("", "", false, "", current_directory);
    formatting_tool("let a: int = 1", "let a : int = 1", false, "", current_directory);
    formatting_tool("let a :int = 1", "let a : int = 1", false, "", current_directory);
    formatting_tool("let a : int =1", "let a : int = 1", false, "", current_directory);
    formatting_tool("let a : int= 1", "let a : int = 1", false, "", current_directory);
    formatting_tool("loop\n"
                    "    break\n"
                    "match x\n"
                    "    case 1 : 0\n"
                    "    case 2:\n"
                    "        1\n"
                    "let s= struct\n"
                    "    i: unit",
                    "loop\n"
                    "    break\n"
                    "match x\n"
                    "    case 1: 0\n"
                    "    case 2:\n"
                    "        1\n"
                    "\n"
                    "let s = struct\n"
                    "    i: unit",
                    false, "", current_directory);
    formatting_tool("match a\n"
                    "    case 1:2\n",
                    "match a\n"
                    "    case 1: 2\n",
                    false, "", current_directory);
    formatting_tool("let s=import s", "let s = import s", false, "", current_directory);
    formatting_tool("loop\n"
                    "\tbreak\n",
                    "loop\n"
                    "    break",
                    false, "", current_directory);
    //    formatting_tool("loop\n"
    //                    "\tbreak\n"
    //                    "    match a\n"
    //                    "\t    case 1: 1",
    //                    "loop\n"
    //                    "    break\n"
    //                    "    match a:\n"
    //                    "        case 1: 1",
    //                    false, "");
}

static void test_web_cli(unicode_view const current_directory)
{
    unicode_string input = write_temporary_file("");
    unicode_string output = write_temporary_file("");
    char *arguments[] = {"lpg", "web", unicode_string_c_str(&input), unicode_string_c_str(&output)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, false,
                  "Could not open source file\nCould not find template. Using "
                  "default template.",
                  current_directory);
    REQUIRE(0 == remove(unicode_string_c_str(&input)));
    unicode_string_free(&input);
    REQUIRE(0 == remove(unicode_string_c_str(&output)));
    unicode_string_free(&output);
}

static unicode_string find_node_test_main(void)
{
    unicode_view const pieces[] = {path_remove_leaf(unicode_view_from_c_str(__FILE__)), unicode_view_from_c_str("node"),
                                   unicode_view_from_c_str("main.lpg")};
    return path_combine(pieces, LPG_ARRAY_SIZE(pieces));
}

static unicode_string create_temporary_directory(void)
{
#ifdef _MSC_VER
    size_t const size = 300;
    unicode_string result = {allocate(size), size};
    if (tmpnam_s(result.data, result.length))
    {
        LPG_TO_DO();
    }
    result.length = strlen(result.data);
    return result;
#else
    unicode_string result = unicode_string_from_c_str("/tmp/lpg-test-XXXXXX");
    if (!mkdtemp(unicode_string_c_str(&result)))
    {
        LPG_TO_DO();
    }
    return result;
#endif
}

static void remove_directory(unicode_view const removed)
{
    unicode_view const arguments[] = {unicode_view_from_c_str("-rf"), removed};
    create_process_result process =
        create_process(unicode_view_from_c_str("/bin/rm"), arguments, LPG_ARRAY_SIZE(arguments),
                       unicode_view_from_c_str("/"), get_standard_input(), get_standard_output(), get_standard_error());
    if (process.success != success_yes)
    {
        LPG_TO_DO();
    }
    if (wait_for_process_exit(process.created) != 0)
    {
        LPG_TO_DO();
    }
}

void test_cli(void)
{
    unicode_view const current_directory_not_used = unicode_view_from_c_str("/does-not-exist");
    {
        char *arguments[] = {"lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n", current_directory_not_used);
    }
    {
        char *arguments[] = {"lpg", "run"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n", current_directory_not_used);
    }
    {
        char *arguments[] = {"lpg", "unknown"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n", current_directory_not_used);
    }
    {
        char *arguments[] = {"lpg", "run", "not-found"};
        expect_output(
            LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n", current_directory_not_used);
    }
    {
        char *arguments[] = {"lpg", "web", "input.lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n", current_directory_not_used);
    }
    {
        char *arguments[] = {"lpg", "web", "input.lpg", "output.html"};
        expect_output(
            LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n", current_directory_not_used);
    }
    {
        unicode_string project = find_node_test_main();
        char *arguments[] = {"lpg", "node", unicode_string_c_str(&project)};
        unicode_string const working_directory = create_temporary_directory();
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, false, "", unicode_view_from_string(working_directory));
        remove_directory(unicode_view_from_string(working_directory));
        unicode_string_free(&project);
        unicode_string_free(&working_directory);
    }
    test_web_cli(current_directory_not_used);

    expect_output_with_source_flags("assert(boolean.true)\n", "compile", false, "", current_directory_not_used);
    expect_output_with_source_flags("assert(boolean.true)\n", "run", false, "", current_directory_not_used);
    expect_output_with_source_flags("", "unknown", true,
                                    "Arguments: [run|format|compile|web] filename [web output file]\n",
                                    current_directory_not_used);

    expect_output_with_source_flags("print(\"Hello World)\n", "compile", true,
                                    "Invalid token in line 1:\nprint(\"Hello World)\n      ^\nExpected "
                                    "expression "
                                    "in line 1:\nprint(\"Hello World)\n                   ^\nExpected "
                                    "expression "
                                    "in line 2:\n\n^\nExpected arguments in line 2:\n\n^\n",
                                    current_directory_not_used);

    expect_output_with_source("print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Invalid token in line 3:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 3:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              current_directory_not_used);
    expect_output_with_source("\n"
                              "print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              current_directory_not_used);
    expect_output_with_source("\r\n"
                              "print(\"\")\r\n"
                              "print(a)\r\n"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              current_directory_not_used);
    expect_output_with_source("\r"
                              "assert(b)\r"
                              "assert(a)\r"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              current_directory_not_used);
    expect_output_with_source("assert(a)\n"
                              "assert(b)\n"
                              "assert(c)\n",
                              true, "Unknown structure element or global identifier in line 1:\n"
                                    "assert(a)\n"
                                    "       ^\n"
                                    "Unknown structure element or global identifier in line 2:\n"
                                    "assert(b)\n"
                                    "       ^\n"
                                    "Unknown structure element or global identifier in line 3:\n"
                                    "assert(c)\n"
                                    "       ^\n",
                              current_directory_not_used);
    expect_output_with_source("syntax error here", true, "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "      ^\n"
                                                         "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "            ^\n",
                              current_directory_not_used);
    expect_output_with_source("unknown_identifier()", true,
                              "Unknown structure element or global identifier in line 1:\n"
                              "unknown_identifier()\n"
                              "^\n",
                              current_directory_not_used);
    expect_output_with_source("assert(boolean.true)\nassert(a)", true,
                              "Unknown structure element or global identifier in line 2:\n"
                              "assert(a)\n"
                              "       ^\n",
                              current_directory_not_used);
    expect_output_with_source("xor(boolean.true, boolean.false)\n"
                              "let xor = (left: boolean, right: boolean) assert(boolean.false)\n",
                              true, "Unknown structure element or global identifier in line 1:\n"
                                    "xor(boolean.true, boolean.false)\n"
                                    "^\n",
                              current_directory_not_used);

    expect_output_with_source("import unknown\n", true, "import failed in line 1:\n"
                                                        "import unknown\n"
                                                        "^\n",
                              current_directory_not_used);

    expect_output_with_source("let std = import std\n", false, "", current_directory_not_used);

    test_formatting_tool(current_directory_not_used);
}
