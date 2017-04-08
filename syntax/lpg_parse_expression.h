#pragma once
#include "lpg_expression.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_view.h"

typedef size_t line_number;
typedef size_t column_number;

typedef struct source_location
{
    line_number line;
    column_number approximate_column;
} source_location;

int source_location_equals(source_location left, source_location right);

typedef struct rich_token
{
    tokenize_status status;
    token_type token;
    unicode_view content;
    source_location where;
} rich_token;

int is_end_of_file(rich_token const *token);

typedef struct parse_error
{
    source_location where;
} parse_error;

parse_error parse_error_create(source_location where);
int parse_error_equals(parse_error left, parse_error right);

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
