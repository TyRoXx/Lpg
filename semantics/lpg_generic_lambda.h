#pragma once

#include "lpg_expression.h"
#include "lpg_value.h"
#include "lpg_generic_interface.h"

typedef struct generic_lambda
{
    lambda tree;
    generic_enum_closures closures;
} generic_lambda;

generic_lambda generic_lambda_create(lambda tree, generic_enum_closures closures);
void generic_lambda_free(generic_lambda const freed);
