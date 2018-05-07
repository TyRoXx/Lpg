#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"
#include "lpg_generic_enum.h"

typedef struct generic_enum_closures
{
    generic_enum_closure *elements;
    size_t count;
} generic_enum_closures;

void generic_enum_closures_free(generic_enum_closures const freed);

typedef struct generic_interface
{
    interface_expression tree;
    generic_enum_closures closures;
} generic_interface;

generic_interface generic_interface_create(interface_expression tree, generic_enum_closures closures);
void generic_interface_free(generic_interface const freed);
