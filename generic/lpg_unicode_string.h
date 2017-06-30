#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct unicode_string
{
    char *data;
    size_t length;
} unicode_string;

unicode_string unicode_string_from_range(char const *data, size_t length);
unicode_string unicode_string_from_c_str(const char *c_str);
void unicode_string_free(unicode_string const *s);
bool unicode_string_equals(unicode_string const left,
                           unicode_string const right);
char *unicode_string_c_str(unicode_string *value);
