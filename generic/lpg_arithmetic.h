#pragma once
#include "lpg_optional.h"
#include <stddef.h>
#include "lpg_use_result.h"

typedef struct optional_size
{
    optional state;
    size_t value_if_set;
} optional_size;

LPG_USE_RESULT optional_size make_optional_size(size_t const value);

static optional_size const optional_size_empty = {optional_empty, 0};

LPG_USE_RESULT optional_size size_multiply(size_t const first,
                                           size_t const second);
