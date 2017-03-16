#pragma once
#include "lpg_optional.h"
#include <stddef.h>

typedef struct optional_size
{
    optional state;
    size_t value_if_set;
} optional_size;

static optional_size make_optional_size(size_t const value)
{
    optional_size result = {optional_set, value};
    return result;
}

static optional_size const optional_size_empty = {optional_empty, 0};

static optional_size size_multiply(size_t const first, size_t const second)
{
    size_t const product = first * second;
    if ((first != 0) && (product / first) != second)
    {
        return optional_size_empty;
    }
    return make_optional_size(product);
}
