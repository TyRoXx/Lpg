#pragma once
#include "lpg_parse_expression.h"
#include "lpg_source_location.h"

typedef struct parser_user
{
    char const *remaining_input;
    size_t remaining_size;
    source_location current_location;
} parser_user;

rich_token find_next_token(callback_user user);
