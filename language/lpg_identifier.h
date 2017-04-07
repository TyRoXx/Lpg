#pragma once
#include "lpg_unicode_string.h"

int is_identifier_begin(unicode_code_point const c);
int is_identifier_middle(unicode_code_point const c);
int is_identifier(unicode_code_point const *data, size_t size);
