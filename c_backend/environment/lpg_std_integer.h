#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lpg_std_string.h"
#include <stdio.h>

static bool integer_equals(uint64_t const left, uint64_t const right)
{
    return (left == right);
}

static bool integer_less(uint64_t const left, uint64_t const right)
{
    return (left < right);
}

static string_ref integer_to_string(uint64_t const value)
{
    char buffer[40];
    int const formatted_length =
#ifdef _MSC_VER
        sprintf_s
#else
        sprintf
#endif
        (buffer,
#ifdef _MSC_VER
         sizeof(buffer),
#endif
         "%llu", (unsigned long long)value);
    return string_ref_concat(string_literal("", 0), string_literal(buffer, (size_t)formatted_length));
}
