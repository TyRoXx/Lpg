#pragma once

#include "lpg_expression.h"

typedef struct generic_enum
{
    enum_expression tree;
} generic_enum;

generic_enum generic_enum_create(enum_expression tree);
