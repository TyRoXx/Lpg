#include "test_cli.h"
#include "test.h"
#include "lpg_cli.h"
#include "lpg_array_size.h"
#include "lpg_allocate.h"
#include "lpg_read_file.h"
#include "lpg_write_file.h"
#include "find_builtin_module_directory.h"
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
    char *arguments[] = {"lpg", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void expect_output_with_source_flags(char const *const source, char **const flags, const size_t flag_count,
                                            bool const expected_exit_code, char const *const expected_diagnostics)
{
    unicode_string name = write_temporary_file(source);
    size_t new_size = 2 + flag_count;
    char **arguments = allocate_array(new_size, sizeof(arguments));
    arguments[0] = "lpg";
    arguments[1] = unicode_string_c_str(&name);
    for (size_t i = 2; i < new_size; ++i)
    {
        arguments[i] = flags[i - 2];
    }
    expect_output((int)new_size, arguments, expected_exit_code, expected_diagnostics);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
    deallocate(arguments);
}

static void formatting_tool(char const *const source, char const *const expected_output, bool const expected_exit_code,
                            char const *const expected_diagnostics)
{
    unicode_string name = write_temporary_file(source);
    char *arguments[] = {"lpg", unicode_string_c_str(&name), "--format"};
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
                    "    case 1: 0\n"
                    "    case 2:\n"
                    "        1\n"
                    "let s = struct\n"
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
}

void test_cli(void)
{
    {
        char *arguments[] = {"lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Arguments: filename\n");
    }
    {
        char *arguments[] = {"lpg", "not-found"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n");
    }

    {
        char *flags[] = {"--compile-only"};
        expect_output_with_source_flags("assert(boolean.true)\n", flags, 1, false, "");
    }

    expect_output_with_source_flags("assert(boolean.true)\n", NULL, 0, false, "");

    {
        char *flags[] = {"--unknown-flag"};
        expect_output_with_source_flags("", flags, 1, true, "Arguments: filename\n");
    }

    {
        char *flags[] = {"--compile-only"};
        expect_output_with_source_flags("print(\"Hello World)\n", flags, 1, true,
                                        "Invalid token in line 1:\nprint(\"Hello World)\n      ^\nExpected expression "
                                        "in line 1:\nprint(\"Hello World)\n                   ^\nExpected expression "
                                        "in line 2:\n\n^\nExpected arguments in line 2:\n\n^\n");
    }

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
