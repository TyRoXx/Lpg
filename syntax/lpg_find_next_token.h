#pragma once
#include "lpg_source_location.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_view.h"

typedef struct parser_user
{
    char const *remaining_input;
    size_t remaining_size;
    source_location current_location;
} parser_user;

typedef struct rich_token
{
    tokenize_status status;
    token_type token;
    unicode_view content;
    source_location where;
} rich_token;

rich_token find_next_token(parser_user *user);
