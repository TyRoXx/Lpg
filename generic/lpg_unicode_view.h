#pragma once
#include "lpg_unicode_string.h"

typedef struct unicode_view
{
    char const *begin;
    size_t length;
} unicode_view;

unicode_view unicode_view_create(char const *begin, size_t length);
unicode_view unicode_view_from_c_str(LPG_NON_NULL(char const *c_str));
unicode_view unicode_view_from_string(unicode_string string);
bool unicode_view_equals_c_str(unicode_view left,
                               LPG_NON_NULL(char const *right));
bool unicode_view_equals(unicode_view left, unicode_view right);
unicode_string unicode_view_copy(unicode_view value);
unicode_view unicode_view_cut(unicode_view const whole, size_t const begin,
                              size_t const end);
