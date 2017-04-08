#include "lpg_parse_expression.h"
#include "lpg_assert.h"

source_location source_location_create(line_number line,
                                       column_number approximate_column)
{
    source_location result = {line, approximate_column};
    return result;
}

int source_location_equals(source_location left, source_location right)
{
    return (left.line == right.line) &&
           (left.approximate_column == right.approximate_column);
}

rich_token rich_token_create(tokenize_status status, token_type token,
                             unicode_view content, source_location where)
{
    rich_token const result = {status, token, content, where};
    return result;
}

int is_end_of_file(rich_token const *token)
{
    return (token->status == tokenize_success) && (token->content.length == 0);
}

parse_error parse_error_create(source_location where)
{
    parse_error result = {where};
    return result;
}

int parse_error_equals(parse_error left, parse_error right)
{
    return source_location_equals(left.where, right.where);
}

expression_parser expression_parser_create(rich_token_producer find_next_token,
                                           parse_error_handler on_error,
                                           callback_user user)
{
    expression_parser result = {find_next_token,
                                on_error,
                                user,
                                0,
                                {tokenize_success, 0, {NULL, 0}, {0, 0}}};
    return result;
}

static rich_token peek(expression_parser *parser)
{
    if (parser->has_cached_token)
    {
        return parser->cached_token;
    }
    parser->cached_token = parser->find_next_token(parser->user);
    parser->has_cached_token = 1;
    return parser->cached_token;
}

static void pop(expression_parser *parser)
{
    ASSUME(parser->has_cached_token);
    parser->has_cached_token = 0;
}

expression_parser_result parse_expression(expression_parser *parser)
{
    for (;;)
    {
        rich_token const head = peek(parser);
        if (is_end_of_file(&head))
        {
            pop(parser);
            parser->on_error(parse_error_create(head.where), parser->user);
            expression_parser_result const result = {
                0, expression_from_break()};
            return result;
        }
        switch (head.status)
        {
        case tokenize_success:
            switch (head.token)
            {
            case token_identifier:
                if (unicode_view_equals_c_str(head.content, "break"))
                {
                    expression_parser_result result = {
                        1, expression_from_break()};
                    return result;
                }
                else
                {
                    expression_parser_result result = {
                        1, expression_from_identifier(
                               unicode_view_copy(head.content))};
                    return result;
                }

            case token_newline:
            case token_space:
            case token_indentation:
                pop(parser);
                break;

            case token_operator:
                pop(parser);
                parser->on_error(parse_error_create(head.where), parser->user);
                break;
            }
            break;

        case tokenize_invalid:
        {
            expression_parser_result const result = {
                0, expression_from_break()};
            return result;
        }
        }
    }
}
