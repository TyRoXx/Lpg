#pragma once
#include "lpg_unicode_string.h"

typedef struct unicode_view
{
    unicode_code_point const *begin;
    size_t length;
} unicode_view;

unicode_view unicode_view_create(unicode_code_point const *begin,
                                 size_t length);
unicode_view unicode_view_from_string(unicode_string string);
int unicode_view_equals_c_str(unicode_view left, char const *right);
int unicode_view_equals(unicode_view left, unicode_view right);
unicode_string unicode_view_copy(unicode_view value);
