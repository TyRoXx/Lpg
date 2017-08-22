#pragma once
#include "lpg_blob.h"

typedef struct unicode_string
{
    char *data;
    size_t length;
} unicode_string;

unicode_string unicode_string_from_range(char const *data,
                                         size_t length) LPG_USE_RESULT;
unicode_string
unicode_string_from_c_str(LPG_NON_NULL(const char *c_str)) LPG_USE_RESULT;
void unicode_string_free(LPG_NON_NULL(unicode_string const *s));
bool unicode_string_equals(unicode_string const left,
                           unicode_string const right) LPG_USE_RESULT;
char *unicode_string_c_str(LPG_NON_NULL(unicode_string *value)) LPG_USE_RESULT;
unicode_string unicode_string_validate(blob const raw) LPG_USE_RESULT;
