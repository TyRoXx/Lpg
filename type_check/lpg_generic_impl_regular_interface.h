#pragma once

#include "lpg_generic_impl.h"

typedef struct generic_impl_regular_interface
{
    interface_id interface_;
    impl_expression tree;
    generic_closures closures;
    generic_instantiation_expression self;
} generic_impl_regular_interface;

generic_impl_regular_interface generic_impl_regular_interface_create(interface_id interface_, impl_expression tree,
                                                                     generic_closures closures,
                                                                     generic_instantiation_expression self);
void generic_impl_regular_interface_free(generic_impl_regular_interface const freed);
