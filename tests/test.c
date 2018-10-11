#include "test.h"
#include "lpg_atomic.h"
#include <stdio.h>

static size_t total_checks;
static size_t failures;

bool lpg_check(bool const success)
{
    atomic_size_increment(&total_checks);
    if (!success)
    {
        atomic_size_increment(&failures);
    }
    return success;
}

int lpg_print_test_summary(void)
{
    printf("%zu of %zu checks failed\n", failures, total_checks);
    return failures != 0;
}
