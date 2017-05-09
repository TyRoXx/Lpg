#include "test_cli.h"
#include "test.h"
#include "lpg_cli.h"
#include "lpg_array_size.h"
#include "lpg_unicode_string.h"
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

static void expect_output(int argc, char **argv, bool const expected_exit_code,
                          char const *const expected_diagnostics,
                          char const *const expected_print_output)
{
    memory_writer diagnostics = {NULL, 0, 0};
    memory_writer print_output = {NULL, 0, 0};
    REQUIRE(expected_exit_code == run_cli(argc, argv,
                                          memory_writer_erase(&diagnostics),
                                          memory_writer_erase(&print_output)));
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    REQUIRE(memory_writer_equals(print_output, expected_print_output));
    memory_writer_free(&print_output);
    memory_writer_free(&diagnostics);
}

static void expect_output_with_source(char const *const source,
                                      bool const expected_exit_code,
                                      char const *const expected_diagnostics,
                                      char const *const expected_print_output)
{
    unicode_string name = write_file(source);
    char *arguments[] = {"lpg", unicode_string_c_str(&name)};
    expect_output(LPG_ARRAY_SIZE(arguments), arguments, expected_exit_code,
                  expected_diagnostics, expected_print_output);
    unicode_string_free(&name);
}

void test_cli(void)
{
    {
        char *arguments[] = {"lpg"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Arguments: filename\n", "");
    }
    {
        char *arguments[] = {"lpg", "not-found"};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Could not open source file\n", "");
    }
    expect_output_with_source(
        "print(\"Hello, world!\")", false, "", "Hello, world!");
    expect_output_with_source("syntax error here", true,
                              "Expected declaration or assignment in line 1\n"
                              "Expected expression in line 1\n",
                              "");
    expect_output_with_source(
        "unknown_identifier()", true, "Unknown identifier in line 1\n", "");
}
