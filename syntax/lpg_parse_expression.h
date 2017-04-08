#pragma once
#include "lpg_expression.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_view.h"

typedef struct rich_token
{
    tokenize_status status;
    token_type token;
    unicode_view content;
} rich_token;

int is_end_of_file(rich_token const *token);

typedef size_t line_number;
typedef size_t column_number;

typedef struct parse_error
{
    line_number line;
    column_number approximate_column;
} parse_error;

typedef enum continue_flag
{
    continue_yes,
    continue_no
} continue_flag;

typedef void *callback_user;

typedef rich_token (*rich_token_producer)(callback_user);
typedef continue_flag (*parse_error_handler)(parse_error, callback_user);

typedef struct expression_parser
{
    rich_token_producer find_next_token;
    parse_error_handler on_error;
    callback_user user;
    int has_cached_token;
    rich_token cached_token;
} expression_parser;

expression_parser expression_parser_create(rich_token_producer find_next_token,
                                           parse_error_handler on_error,
                                           callback_user user);

typedef struct expression_parser_result
{
    int is_success;
    expression success;
} expression_parser_result;

expression_parser_result parse_expression(expression_parser *parser);
