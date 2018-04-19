#pragma once

#include "lpg_value.h"

typedef struct module_loader
{
    void *dummy;
} module_loader;

module_loader module_loader_create(void);

typedef struct load_module_result
{
    optional_value loaded;
    type schema;
} load_module_result;

load_module_result load_module(LPG_NON_NULL(module_loader *loader), unicode_view name);
