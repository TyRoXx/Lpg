#pragma once

#include "lpg_generic_enum.h"
#include "lpg_generic_enum_id.h"
#include "lpg_generic_enum_instantiation.h"
#include "lpg_module.h"
#include "lpg_load_module.h"

typedef struct program_check
{
    generic_enum *generic_enums;
    generic_enum_id generic_enum_count;
    generic_enum_instantiation *enum_instantiations;
    size_t enum_instantiation_count;
    module *modules;
    size_t module_count;
    module_loader *loader;
} program_check;

void program_check_free(program_check const freed);
void add_module(program_check *to, module added);
