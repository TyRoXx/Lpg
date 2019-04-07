#pragma once

#include "lpg_generic_interface.h"

typedef struct generic_struct
{
    struct_expression tree;
    generic_closures closures;
    unicode_string current_import_directory;
} generic_struct;

generic_struct generic_struct_create(struct_expression tree, generic_closures closures,
                                     unicode_string current_import_directory);
void generic_struct_free(generic_struct const freed);
