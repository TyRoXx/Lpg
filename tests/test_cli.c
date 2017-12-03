#include "test_cli.h"
#include "test.h"
#include "lpg_cli.h"
#include "lpg_array_size.h"
#include "lpg_allocate.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static unicode_string write_file(char const *const content)
{
#ifdef _WIN32
    char *const name =
#ifdef _MSC_VER
        _tempnam
#else
        tempnam
#endif
        (NULL, NULL);
    REQUIRE(name);
    size_t const name_length = strlen(name);
    unicode_string const result = {allocate(name_length), name_length};
    memcpy(result.data, name, name_length);
    FILE *file = NULL;
    fopen_s(&file, name, "wb");
    REQUIRE(file);
    REQUIRE(strlen(content) == fwrite(content, 1, strlen(content), file));
    fclose(file);
    free(name);
    return result;
#else
    char name[] = "/tmp/XXXXXX";
    int const file = mkstemp(name);
    REQUIRE(file >= 0);
    REQUIRE(write(file, content, strlen(content)) == (ssize_t)strlen(content));
    close(file);
    size_t const name_length = strlen(name);
    unicode_string const result = {allocate(name_length), name_length};
    memcpy(result.data, name, name_length);
    return result;
#endif
}

static void expect_output(int argc, char **argv, bool const expected_exit_code, char const *const expected_diagnostics)
{
    memory_writer diagnostics = {NULL, 0, 0};
    bool real_error_code = run_cli(argc, argv, memory_writer_erase(&diagnostics));
    REQUIRE(expected_exit_code == real_error_code);
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    memory_writer_free(&diagnostics);
}

static void expect_output_with_source(char const *const source, bool const expected_exit_code,
                                      char const *const expected_diagnostics)
{
    unicode_string name = write_file(source);
    char *arguments[] = {"lpg", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

static void expect_output_with_source_flags(char const *const source, char **const flags, const size_t flag_count,
                                            bool const expected_exit_code, char const *const expected_diagnostics)
{
    unicode_string name = write_file(source);
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
}
