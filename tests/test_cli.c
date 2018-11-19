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

static void expect_output(int argc, char **argv, bool const expected_exit_code, char const *const expected_diagnostics)
{
    unicode_string const module_directory = find_builtin_module_directory();
    memory_writer diagnostics = {NULL, 0, 0};
    bool real_error_code =
        run_cli(argc, argv, memory_writer_erase(&diagnostics), unicode_view_from_string(module_directory));
    REQUIRE(expected_exit_code == real_error_code);
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    memory_writer_free(&diagnostics);
    unicode_string_free(&module_directory);
}

static void expect_output_with_source(char const *const source, bool const expected_exit_code,
                                      char const *const expected_diagnostics)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[] = {"lpg", "run", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void expect_output_with_source_flags(char const *const source, char *const command,
                                            bool const expected_exit_code, char const *const expected_diagnostics)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[3];
    arguments[0] = "lpg";
    arguments[1] = command;
    arguments[2] = unicode_string_c_str(&name);
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void formatting_tool(char const *const source, char const *const expected_output, bool const expected_exit_code,
                            char const *const expected_diagnostics)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[] = {"lpg", "format", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics);
    blob_or_error const read = read_file(unicode_string_c_str(&name));
    REQUIRE(!read.error);
    REQUIRE(unicode_view_equals_c_str(unicode_view_create(read.success.data, read.success.length), expected_output));
    blob_free(&read.success);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void test_formatting_tool()
{
    formatting_tool("", "", false, "");
    formatting_tool("let a: int = 1", "let a : int = 1", false, "");
    formatting_tool("let a :int = 1", "let a : int = 1", false, "");
    formatting_tool("let a : int =1", "let a : int = 1", false, "");
    formatting_tool("let a : int= 1", "let a : int = 1", false, "");
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
                    false, "");
    formatting_tool("match a\n"
                    "    case 1:2\n",
                    "match a\n"
                    "    case 1: 2\n",
                    false, "");
    formatting_tool("let s=import s", "let s = import s", false, "");
    formatting_tool("loop\n"
                    "\tbreak\n",
                    "loop\n"
                    "    break",
                    false, "");
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

static void test_web_cli(void)
{
    unicode_string input = write_temporary_file("");
    unicode_string output = write_temporary_file("");
    char *arguments[] = {"lpg", "web", unicode_string_c_str(&input), unicode_string_c_str(&output)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, false,
                  "Could not open source file\nCould not find template. Using default template.");
    REQUIRE(0 == remove(unicode_string_c_str(&input)));
    unicode_string_free(&input);
    REQUIRE(0 == remove(unicode_string_c_str(&output)));
    unicode_string_free(&output);
}

void test_cli(void)
{
    {
        char *arguments[] = {"lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n");
    }
    {
        char *arguments[] = {"lpg", "run"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n");
    }
    {
        char *arguments[] = {"lpg", "unknown"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n");
    }
    {
        char *arguments[] = {"lpg", "run", "not-found"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n");
    }
    {
        char *arguments[] = {"lpg", "web", "input.lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: [run|format|compile|web] filename [web output file]\n");
    }
    {
        char *arguments[] = {"lpg", "web", "input.lpg", "output.html"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n");
    }
    test_web_cli();

    expect_output_with_source_flags("assert(boolean.true)\n", "compile", false, "");
    expect_output_with_source_flags("assert(boolean.true)\n", "run", false, "");
    expect_output_with_source_flags(
        "", "unknown", true, "Arguments: [run|format|compile|web] filename [web output file]\n");

    expect_output_with_source_flags("print(\"Hello World)\n", "compile", true,
                                    "Invalid token in line 1:\nprint(\"Hello World)\n      ^\nExpected "
                                    "expression "
                                    "in line 1:\nprint(\"Hello World)\n                   ^\nExpected "
                                    "expression "
                                    "in line 2:\n\n^\nExpected arguments in line 2:\n\n^\n");

    expect_output_with_source("print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Invalid token in line 3:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 3:\n"
                                    "let v = ?\n"
                                    "         ^\n");
    expect_output_with_source("\n"
                              "print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n");
    expect_output_with_source("\r\n"
                              "print(\"\")\r\n"
                              "print(a)\r\n"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n");
    expect_output_with_source("\r"
                              "assert(b)\r"
                              "assert(a)\r"
                              "let v = ?",
                              true, "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n");
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
                                    "       ^\n");
    expect_output_with_source("syntax error here", true, "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "      ^\n"
                                                         "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "            ^\n");
    expect_output_with_source("unknown_identifier()", true,
                              "Unknown structure element or global identifier in line 1:\n"
                              "unknown_identifier()\n"
                              "^\n");
    expect_output_with_source("assert(boolean.true)\nassert(a)", true,
                              "Unknown structure element or global identifier in line 2:\n"
                              "assert(a)\n"
                              "       ^\n");
    expect_output_with_source("xor(boolean.true, boolean.false)\n"
                              "let xor = (left: boolean, right: boolean) assert(boolean.false)\n",
                              true, "Unknown structure element or global identifier in line 1:\n"
                                    "xor(boolean.true, boolean.false)\n"
                                    "^\n");

    expect_output_with_source("import unknown\n", true, "import failed in line 1:\n"
                                                        "import unknown\n"
                                                        "^\n");

    expect_output_with_source("let std = import std\n", false, "");

    test_formatting_tool();
}
