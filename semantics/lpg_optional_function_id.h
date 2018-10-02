#pragma once
#include "lpg_function_id.h"
#include <stdbool.h>

typedef struct optional_function_id
{
    bool is_set;
    function_id value;
} optional_function_id;

optional_function_id optional_function_id_create(function_id value);
optional_function_id optional_function_id_empty(void);
