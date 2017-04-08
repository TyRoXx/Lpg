#include "lpg_parse_expression.h"
#include "lpg_assert.h"

int is_end_of_file(rich_token const *token)
{
    return (token->status == tokenize_success) && (token->content.length == 0);
}

expression_parser expression_parser_create(rich_token_producer find_next_token,
                                           parse_error_handler on_error,
                                           callback_user user)
{
    expression_parser result = {
        find_next_token, on_error, user, 0, {token_space, 0, {NULL, 0}}};
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

static expression_parser_result handle_first_token(expression_parser *parser,
                                                   token_type const token,
                                                   unicode_view const content)
{
    (void)parser;
    switch (token)
    {
    case token_identifier:
        if (unicode_view_equals_c_str(content, "break"))
        {
            expression_parser_result result = {1, expression_from_break()};
            return result;
        }
        else
        {
            expression_parser_result result = {
                1, expression_from_identifier(unicode_view_copy(content))};
            return result;
        }

    case token_newline:
    case token_space:
    case token_indentation:
        UNREACHABLE();

    case token_operator:
    {
        expression_parser_result result = {0, expression_from_break()};
        return result;
    }
    }
    UNREACHABLE();
}

expression_parser_result parse_expression(expression_parser *parser)
{
    rich_token const head = peek(parser);
    switch (head.status)
    {
    case tokenize_success:
        return handle_first_token(parser, head.token, head.content);

    case tokenize_invalid:
    {
        expression_parser_result const result = {0, expression_from_break()};
        return result;
    }
    }
    UNREACHABLE();
}
