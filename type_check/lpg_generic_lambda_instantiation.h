#pragma once
#include "lpg_value.h"

typedef struct generic_lambda_instantiation
{
    generic_lambda_id generic;
    value *arguments;
    size_t argument_count;
    function_id instantiated;
} generic_lambda_instantiation;

generic_lambda_instantiation generic_lambda_instantiation_create(generic_lambda_id generic, value *arguments,
                                                                 size_t argument_count, function_id instantiated);
void generic_lambda_instantiation_free(generic_lambda_instantiation const freed);
