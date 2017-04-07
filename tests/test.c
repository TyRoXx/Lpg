#include "test.h"
#include <stdio.h>

static size_t total_checks;
static size_t failures;

int lpg_check(int success)
{
    ++total_checks;
    failures += !success;
    return success;
}

int lpg_print_test_summary(void)
{
    if (failures)
    {
        printf("%zu of %zu checks failed\n", failures, total_checks);
        return 1;
    }
    printf("All %zu checks succeeded\n", total_checks);
    return 0;
}
