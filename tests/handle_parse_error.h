#pragma once
#include "lpg_find_next_token.h"
#include "lpg_parse_expression.h"

typedef struct test_parser_user
{
    parser_user base;
    parse_error const *expected_errors;
    size_t expected_count;
} test_parser_user;

void handle_error(parse_error const error, callback_user user);
