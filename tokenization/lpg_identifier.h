#pragma once
#include "lpg_unicode_string.h"

bool is_identifier_begin(char const c);
bool is_identifier_middle(char const c);
bool is_identifier(char const *data, size_t size);
