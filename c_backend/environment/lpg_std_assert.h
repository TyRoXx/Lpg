#pragma once
#include "lpg_std_unit.h"
#include <stdlib.h>
#include <stdbool.h>
static unit assert_impl(bool const condition)
{
    if (!condition)
    {
        abort();
    }
    return unit_impl;
}
