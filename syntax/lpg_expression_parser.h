#pragma once

#include "lpg_find_next_token.h"

typedef enum parse_error_type {
    parse_error_invalid_token = 1,
    parse_error_expected_expression,
    parse_error_expected_arguments,
    parse_error_integer_literal_out_of_range,
    parse_error_expected_newline,
    parse_error_expected_declaration_or_assignment,
    parse_error_expected_space,
    parse_error_expected_comma,
    parse_error_expected_colon,
    parse_error_expected_case,
    parse_error_expected_element_name,
    parse_error_expected_identifier,
    parse_error_expected_lambda_body,
    parse_error_unknown_binary_operator,
    parse_error_expected_parameter_list,
    parse_error_expected_for,
    parse_error_expected_left_parenthesis,
    parse_error_expected_right_parenthesis,
    parse_error_expected_right_bracket
} parse_error_type;

typedef struct parse_error
{
    parse_error_type type;
    source_location where;
} parse_error;

parse_error parse_error_create(parse_error_type type, source_location where);
bool parse_error_equals(parse_error left, parse_error right);

typedef rich_token (*rich_token_producer)(callback_user);
typedef void (*parse_error_handler)(parse_error, callback_user);

typedef struct expression_parser
{
    rich_token_producer find_next_token;
    callback_user find_next_token_user;
    parse_error_handler on_error;
    callback_user on_error_user;
    rich_token cached_tokens[3];
    size_t cached_token_count;
} expression_parser;

expression_parser expression_parser_create(LPG_NON_NULL(rich_token_producer find_next_token),
                                           callback_user find_next_token_user,
                                           LPG_NON_NULL(parse_error_handler on_error), callback_user on_error_user);
bool expression_parser_has_remaining_non_empty_tokens(LPG_NON_NULL(expression_parser const *const parser));

rich_token peek_at(expression_parser *parser, size_t const offset);
rich_token peek(expression_parser *parser);

void pop_n(expression_parser *parser, size_t const count);
void pop(expression_parser *parser);
