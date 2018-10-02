#include "lpg_optional_function_id.h"

optional_function_id optional_function_id_create(function_id value)
{
    optional_function_id const result = {true, value};
    return result;
}

optional_function_id optional_function_id_empty(void)
{
    optional_function_id const result = {false, ~(function_id)0};
    return result;
}
