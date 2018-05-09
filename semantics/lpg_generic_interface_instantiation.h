#pragma once
#include "lpg_value.h"

typedef struct generic_interface_instantiation
{
    generic_interface_id generic;
    value *arguments;
    size_t argument_count;
    interface_id instantiated;
} generic_interface_instantiation;

generic_interface_instantiation generic_interface_instantiation_create(generic_interface_id generic, value *arguments,
                                                                       size_t argument_count,
                                                                       interface_id instantiated);
void generic_interface_instantiation_free(generic_interface_instantiation const freed);
