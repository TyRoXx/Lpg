#include "fuzz_target.h"
#include "lpg_allocate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(uint8_t const *const data, size_t const size)
{
    size_t const allocations_before = count_active_allocations();
    ParserTypeCheckerFuzzTarget(data, size);
    size_t const allocations_after = count_active_allocations();
    if (allocations_after != allocations_before)
    {
        fprintf(stderr, "leak detected\n");
        abort();
    }
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
