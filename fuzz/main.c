#include "fuzz_target.h"
#include <string.h>

int LLVMFuzzerTestOneInput(uint8_t const *const data, size_t const size)
{
    ParserTypeCheckerFuzzTarget(data, size);
    return 0;
}

#if !LPG_WITH_CLANG_FUZZER
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char const *const input = "let a = 1";
    LLVMFuzzerTestOneInput((uint8_t const *)input, strlen(input));
    return 0;
}
#endif
