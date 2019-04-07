#pragma once

#include "lpg_expression.h"
#include "lpg_generic_interface.h"
#include "lpg_value.h"

typedef struct generic_enum
{
    enum_expression tree;
    generic_closures closures;
    unicode_string current_import_directory;
} generic_enum;

generic_enum generic_enum_create(enum_expression tree, generic_closures closures,
                                 unicode_string current_import_directory);
void generic_enum_free(generic_enum const freed);
