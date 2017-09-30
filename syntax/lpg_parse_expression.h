#pragma once
#include "lpg_source_location.h"
#include "lpg_expression.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_view.h"

typedef struct rich_token
{
    tokenize_status status;
    token_type token;
    unicode_view content;
    source_location where;
} rich_token;

rich_token rich_token_create(tokenize_status status, token_type token,
                             unicode_view content, source_location where);
bool is_end_of_file(LPG_NON_NULL(rich_token const *token));

typedef enum parse_error_type
{
    parse_error_invalid_token,
    parse_error_expected_expression,
    parse_error_expected_arguments,
    parse_error_integer_literal_out_of_range,
    parse_error_expected_newline,
    parse_error_expected_assignment,
    parse_error_expected_declaration_or_assignment,
    parse_error_expected_space,
    parse_error_expected_comma,
    parse_error_expected_colon,
    parse_error_expected_case,
    parse_error_expected_element_name,
    parse_error_expected_identifier,
    parse_error_expected_lambda_body,
    parse_error_unknown_binary_operator
} parse_error_type;

typedef struct parse_error
{
    parse_error_type type;
    source_location where;
} parse_error;

parse_error parse_error_create(parse_error_type type, source_location where);
bool parse_error_equals(parse_error left, parse_error right);

typedef void *callback_user;

typedef rich_token (*rich_token_producer)(callback_user);
typedef void (*parse_error_handler)(parse_error, callback_user);

typedef struct expression_parser
{
    rich_token_producer find_next_token;
    parse_error_handler on_error;
    callback_user user;
    bool has_cached_token;
    rich_token cached_token;
} expression_parser;

expression_parser
expression_parser_create(LPG_NON_NULL(rich_token_producer find_next_token),
                         LPG_NON_NULL(parse_error_handler on_error),
                         callback_user user);

typedef struct expression_parser_result
{
    bool is_success;
    expression success;
} expression_parser_result;

expression_parser_result
parse_expression(LPG_NON_NULL(expression_parser *const parser),
                 size_t const indentation, bool const may_be_statement);

sequence parse_program(LPG_NON_NULL(expression_parser *parser));
