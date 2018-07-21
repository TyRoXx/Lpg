#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"
#include "lpg_generic_impl.h"

typedef struct generic_interface
{
    interface_expression tree;
    generic_closures closures;
    generic_impl *generic_impls;
    size_t generic_impl_count;
} generic_interface;

generic_interface generic_interface_create(interface_expression tree, generic_closures closures);
void generic_interface_free(generic_interface const freed);
