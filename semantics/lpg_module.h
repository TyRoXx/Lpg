#pragma once

#include "lpg_value.h"

typedef struct module
{
    unicode_string name;
    value content;
    type schema;
} module;

module module_create(unicode_string name, value content, type schema);
void module_free(module const freed);
