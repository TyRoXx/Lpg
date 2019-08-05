#pragma once
#include "lpg_unicode_string.h"

static inline bool is_identifier_begin(char const c)
{
    if ((c >= 'a') && (c <= 'z'))
    {
        return 1;
    }
    if ((c >= 'A') && (c <= 'Z'))
    {
        return 1;
    }
    return (c == '_');
}

static inline bool is_identifier_middle(char const c)
{
    if (is_identifier_begin(c))
    {
        return 1;
    }
    if ((c >= '0') && (c <= '9'))
    {
        return 1;
    }
    return false;
}

bool is_identifier(char const *data, size_t size);
