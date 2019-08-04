#pragma once
#include "lpg_value.h"

typedef struct generic_enum_instantiation
{
    generic_enum_id generic;
    type *argument_types;
    value *arguments;
    size_t argument_count;
    enum_id instantiated;
} generic_enum_instantiation;

generic_enum_instantiation generic_enum_instantiation_create(generic_enum_id generic, type *argument_types,
                                                             value *arguments, size_t argument_count,
                                                             enum_id instantiated);
void generic_enum_instantiation_free(generic_enum_instantiation const freed);
