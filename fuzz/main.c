#include "lpg_cli.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(uint8_t const *const data, size_t const size)
{
    cli_parser_user user = {stream_writer_create_null_writer(), false};
    optional_sequence const result =
        parse(user, unicode_view_from_c_str("fuzzing"), unicode_view_create((char const *)data, size));
    if (result.has_value)
    {
        sequence_free(&result.value);
    }
    return 0;
}

#if !LPG_WITH_CLANG_FUZZER
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char const *const input = "let a = .";
    LLVMFuzzerTestOneInput((uint8_t const *)input, strlen(input));
    return 0;
}
#endif
