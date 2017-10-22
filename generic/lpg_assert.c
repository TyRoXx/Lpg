#include "lpg_assert.h"
#include <stdio.h>

void lpg_to_do(char const *const file, size_t const line)
{
    fprintf(stderr,
            "Encountered LPG_TO_DO() at %s:%zu. You tried to use a feature that has not been implemented yet.\n", file,
            line);
    abort();
}
