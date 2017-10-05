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

static void expect_output(int argc, char **argv, bool const expected_exit_code, char const *const expected_diagnostics,
                          char const *const expected_print_output)
{
    memory_writer diagnostics = {NULL, 0, 0};
    memory_writer print_output = {NULL, 0, 0};
    REQUIRE(expected_exit_code ==
            run_cli(argc, argv, memory_writer_erase(&diagnostics), memory_writer_erase(&print_output)));
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    REQUIRE(memory_writer_equals(print_output, expected_print_output));
    memory_writer_free(&print_output);
    memory_writer_free(&diagnostics);
}

static void expect_output_with_source(char const *const source, bool const expected_exit_code,
                                      char const *const expected_diagnostics, char const *const expected_print_output)
{
    unicode_string name = write_file(source);
    char *arguments[] = {"lpg", unicode_string_c_str(&name)};
    expect_output(
        LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code, expected_diagnostics, expected_print_output);
    REQUIRE(0 == remove(unicode_string_c_str(&name)));
    unicode_string_free(&name);
}

void test_cli(void)
{
    {
        char *arguments[] = {"lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Arguments: filename\n", "");
    }
    {
        char *arguments[] = {"lpg", "not-found"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true, "Could not open source file\n", "");
    }
    expect_output_with_source("print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Invalid token in line 3:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 3:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              "");
    expect_output_with_source("\n"
                              "print(\"\")\n"
                              "print(a)\n"
                              "let v = ?",
                              true, "Expected expression in line 1:\n"
                                    "\n"
                                    "^\n"
                                    "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              "");
    expect_output_with_source("\r\n"
                              "print(\"\")\r\n"
                              "print(a)\r\n"
                              "let v = ?",
                              true, "Expected expression in line 1:\n"
                                    "\n"
                                    "^\n"
                                    "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              "");
    expect_output_with_source("\r"
                              "print(\"\")\r"
                              "print(a)\r"
                              "let v = ?",
                              true, "Expected expression in line 1:\n"
                                    "\n"
                                    "^\n"
                                    "Invalid token in line 4:\n"
                                    "let v = ?\n"
                                    "        ^\n"
                                    "Expected expression in line 4:\n"
                                    "let v = ?\n"
                                    "         ^\n",
                              "");
    expect_output_with_source("print(a)\n"
                              "print(b)\n"
                              "print(c)\n",
                              true, "Unknown structure element or global identifier in line 1:\n"
                                    "print(a)\n"
                                    "      ^\n"
                                    "Unknown structure element or global identifier in line 2:\n"
                                    "print(b)\n"
                                    "      ^\n"
                                    "Unknown structure element or global identifier in line 3:\n"
                                    "print(c)\n"
                                    "      ^\n",
                              "");
    expect_output_with_source("print(\"Hello, world!\\n\")", false, "", "Hello, world!\n");
    expect_output_with_source("syntax error here", true, "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "      ^\n"
                                                         "Expected expression in line 1:\n"
                                                         "syntax error here\n"
                                                         "            ^\n",
                              "");
    expect_output_with_source("unknown_identifier()", true,
                              "Unknown structure element or global identifier in line 1:\n"
                              "unknown_identifier()\n"
                              "^\n",
                              "");
    expect_output_with_source("print(\"\")\nprint(a)", true,
                              "Unknown structure element or global identifier in line 2:\n"
                              "print(a)\n"
                              "      ^\n",
                              "");
}
