#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"
#include "lpg_generic_interface.h"

typedef struct generic_enum
{
    enum_expression tree;
    generic_closures closures;
} generic_enum;

generic_enum generic_enum_create(enum_expression tree, generic_closures closures);
void generic_enum_free(generic_enum const freed);
