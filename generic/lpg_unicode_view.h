#pragma once
#include "lpg_unicode_string.h"
#include "lpg_arithmetic.h"

typedef struct unicode_view
{
    char const *begin;
    size_t length;
} unicode_view;

unicode_view unicode_view_create(char const *begin, size_t length) LPG_USE_RESULT;
unicode_view unicode_view_from_c_str(LPG_NON_NULL(char const *c_str)) LPG_USE_RESULT;
unicode_view unicode_view_from_string(unicode_string string) LPG_USE_RESULT;
bool unicode_view_equals_c_str(unicode_view left, LPG_NON_NULL(char const *right)) LPG_USE_RESULT;
bool unicode_view_equals(unicode_view left, unicode_view right) LPG_USE_RESULT;
bool unicode_view_less(unicode_view const left, unicode_view const right) LPG_USE_RESULT;
unicode_string unicode_view_copy(unicode_view value) LPG_USE_RESULT;
unicode_view unicode_view_cut(unicode_view const whole, size_t const begin, size_t const end) LPG_USE_RESULT;
optional_size unicode_view_find(unicode_view haystack, const char needle) LPG_USE_RESULT;
unicode_string unicode_view_zero_terminate(unicode_view original) LPG_USE_RESULT;
unicode_string unicode_view_concat(unicode_view first, unicode_view second) LPG_USE_RESULT;
