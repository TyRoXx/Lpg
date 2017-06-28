#pragma once
#include "lpg_unicode_string.h"

typedef struct unicode_string_or_error
{
    char const *error;
    unicode_string success;
} unicode_string_or_error;

unicode_string_or_error make_unicode_string_success(unicode_string success);

unicode_string_or_error make_unicode_string_error(char const *const error);

unicode_string_or_error read_file(char const *const name);
