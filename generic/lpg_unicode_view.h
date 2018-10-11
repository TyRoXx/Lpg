#pragma once
#include "lpg_arithmetic.h"
#include "lpg_unicode_string.h"
#include <string.h>

typedef struct unicode_view
{
    char const *begin;
    size_t length;
} unicode_view;

unicode_view unicode_view_create(char const *begin, size_t length) LPG_USE_RESULT;

static inline unicode_view unicode_view_from_c_str(char const *c_str)
{
    return unicode_view_create(c_str, strlen(c_str));
}

unicode_view unicode_view_from_string(unicode_string string) LPG_USE_RESULT;

static inline bool unicode_view_equals_c_str(unicode_view left, char const *right)
{
    size_t const length = strlen(right);
    if (left.length != length)
    {
        return false;
    }
    return !memcmp(left.begin, right, length);
}

bool unicode_view_equals(unicode_view left, unicode_view right) LPG_USE_RESULT;
bool unicode_view_less(unicode_view const left, unicode_view const right) LPG_USE_RESULT;
unicode_string unicode_view_copy(unicode_view value) LPG_USE_RESULT;
unicode_view unicode_view_cut(unicode_view const whole, size_t const begin, size_t const end) LPG_USE_RESULT;
optional_size unicode_view_find(unicode_view haystack, const char needle) LPG_USE_RESULT;
unicode_string unicode_view_zero_terminate(unicode_view original) LPG_USE_RESULT;
unicode_string unicode_view_concat(unicode_view first, unicode_view second) LPG_USE_RESULT;
