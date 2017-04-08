#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint32_t unicode_code_point;

typedef struct unicode_string
{
    unicode_code_point *data;
    size_t length;
} unicode_string;

unicode_string unicode_string_from_range(unicode_code_point const *data,
                                         size_t length);
unicode_string unicode_string_from_c_str(const char *c_str);
void unicode_string_insert(unicode_string *const s, size_t const position,
                           unicode_code_point const inserted);
void unicode_string_erase(unicode_string *const s, size_t const position);
void unicode_string_free(unicode_string const *s);
