#pragma once
#include "lpg_std_unit.h"
#include <stdbool.h>
#include <stdlib.h>
static unit assert_impl(size_t const condition)
{
    if (!condition)
    {
        abort();
    }
    return unit_impl;
}
