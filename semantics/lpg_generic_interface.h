#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"
#include "lpg_generic_enum.h"

typedef struct generic_closures
{
    generic_closure *elements;
    size_t count;
} generic_closures;

void generic_closures_free(generic_closures const freed);

typedef struct generic_interface
{
    interface_expression tree;
    generic_closures closures;
} generic_interface;

generic_interface generic_interface_create(interface_expression tree, generic_closures closures);
void generic_interface_free(generic_interface const freed);
