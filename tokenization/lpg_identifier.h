#pragma once
#include "lpg_unicode_string.h"

int is_identifier_begin(char const c);
int is_identifier_middle(char const c);
int is_identifier(char const *data, size_t size);
