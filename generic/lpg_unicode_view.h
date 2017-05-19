#pragma once
#include "lpg_unicode_string.h"

typedef struct unicode_view
{
    char const *begin;
    size_t length;
} unicode_view;

unicode_view unicode_view_create(char const *begin, size_t length);
unicode_view unicode_view_from_c_str(char const *c_str);
unicode_view unicode_view_from_string(unicode_string string);
bool unicode_view_equals_c_str(unicode_view left, char const *right);
bool unicode_view_equals(unicode_view left, unicode_view right);
unicode_string unicode_view_copy(unicode_view value);
