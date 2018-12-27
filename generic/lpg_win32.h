#pragma once
#include "lpg_unicode_view.h"

#ifdef _WIN32
typedef struct win32_string
{
    wchar_t *c_str;
    size_t size;
} win32_string;

win32_string win32_string_create(wchar_t *c_str, size_t size);
void win32_string_free(win32_string const freed);
win32_string to_win32_path(unicode_view const original);
unicode_string from_win32_string(win32_string const original);
wchar_t *win32_string_two_null_c_str(win32_string *const string);

#endif
