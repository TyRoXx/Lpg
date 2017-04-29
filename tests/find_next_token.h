#pragma once
#include "lpg_source_location.h"
#include "lpg_parse_expression.h"

typedef struct test_parser_user
{
    char const *remaining_input;
    size_t remaining_size;
    parse_error const *expected_errors;
    size_t expected_count;
    source_location current_location;
} test_parser_user;

rich_token find_next_token(callback_user user);
void handle_error(parse_error const error, callback_user user);
