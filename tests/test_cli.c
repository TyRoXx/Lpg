#include "test_cli.h"
#include "test.h"
#include "lpg_cli.h"
#include "lpg_array_size.h"
#include "lpg_unicode_string.h"
#include "lpg_allocate.h"
#include <stdio.h>
#include <string.h>

static unicode_string write_file(char const *const content)
{
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
#ifdef _WIN32
    FILE *file = NULL;
    fopen_s(&file, name, "wb");
#else
    FILE *const file = fopen(name, "wb");
#endif
    REQUIRE(file);
    REQUIRE(strlen(content) == fwrite(content, 1, strlen(content), file));
    fclose(file);
    free(name);
    return result;
}

static void expect_output(int argc, char **argv, bool const expected_exit_code,
                          char const *const expected_diagnostics,
                          char const *const expected_print_output)
{
    memory_writer diagnostics = {NULL, 0};
    memory_writer print_output = {NULL, 0};
    REQUIRE(expected_exit_code == run_cli(argc, argv,
                                          memory_writer_erase(&diagnostics),
                                          memory_writer_erase(&print_output)));
    REQUIRE(memory_writer_equals(diagnostics, expected_diagnostics));
    REQUIRE(memory_writer_equals(print_output, expected_print_output));
    memory_writer_free(&print_output);
    memory_writer_free(&diagnostics);
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
    {
        unicode_string name = write_file("print(\"Hello, world!\")");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        expect_output(
            LPG_ARRAY_SIZE(arguments), arguments, false, "", "Hello, world!");
        unicode_string_free(&name);
    }
    {
        unicode_string name = write_file("syntax error here");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Expected declaration or assignment in line 1\n"
                      "Expected expression in line 1\n",
                      "");
        unicode_string_free(&name);
    }
    {
        unicode_string name = write_file("unknown_identifier()");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        expect_output(LPG_ARRAY_SIZE(arguments), arguments, true,
                      "Unknown identifier in line 1\n", "");
        unicode_string_free(&name);
    }
}
