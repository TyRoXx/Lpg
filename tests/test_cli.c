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

void test_cli(void)
{
    {
        char *arguments[] = {"lpg"};
        REQUIRE(1 == run_cli(LPG_ARRAY_SIZE(arguments), arguments));
    }
    {
        char *arguments[] = {"lpg", "not-found"};
        REQUIRE(1 == run_cli(LPG_ARRAY_SIZE(arguments), arguments));
    }
    {
        unicode_string name = write_file("print(\"Hello, world!\")");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        REQUIRE(0 == run_cli(LPG_ARRAY_SIZE(arguments), arguments));
        unicode_string_free(&name);
    }
    {
        unicode_string name = write_file("syntax error here");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        REQUIRE(1 == run_cli(LPG_ARRAY_SIZE(arguments), arguments));
        unicode_string_free(&name);
    }
    {
        unicode_string name = write_file("unknown_identifier()");
        char *arguments[] = {"lpg", unicode_string_c_str(&name)};
        REQUIRE(1 == run_cli(LPG_ARRAY_SIZE(arguments), arguments));
        unicode_string_free(&name);
    }
}
