#pragma once

#include "lpg_value.h"

typedef struct module
{
    unicode_string name;
    optional_value content;
    optional_type schema;
} module;

module module_create(unicode_string name, optional_value content, optional_type schema);
void module_free(module const freed);
