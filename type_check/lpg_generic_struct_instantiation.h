#pragma once
#include "lpg_value.h"

typedef struct generic_struct_instantiation
{
    generic_struct_id generic;
    value *arguments;
    size_t argument_count;
    struct_id instantiated;
    type *argument_types;
} generic_struct_instantiation;

generic_struct_instantiation generic_struct_instantiation_create(generic_struct_id generic, value *arguments,
                                                                 size_t argument_count, struct_id instantiated,
                                                                 type *argument_types);
void generic_struct_instantiation_free(generic_struct_instantiation const freed);
