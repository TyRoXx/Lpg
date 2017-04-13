#include "lpg_parse_expression.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

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
    while (!parser->has_cached_token)
    {
        parser->cached_token = parser->find_next_token(parser->user);
        switch (parser->cached_token.status)
        {
        case tokenize_success:
            parser->has_cached_token = 1;
            break;

        case tokenize_invalid:
            parser->on_error(
                parse_error_create(parser->cached_token.where), parser->user);
            break;
        }
    }
    ASSUME(parser->cached_token.status == tokenize_success);
    return parser->cached_token;
}

static void pop(expression_parser *parser)
{
    ASSUME(parser->has_cached_token);
    parser->has_cached_token = 0;
}

static expression_parser_result parse_callable(expression_parser *parser)
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
        switch (head.token)
        {
        case token_identifier:
        {
            pop(parser);
            if (unicode_view_equals_c_str(head.content, "break"))
            {
                expression_parser_result result = {1, expression_from_break()};
                return result;
            }
            expression_parser_result result = {
                1, expression_from_identifier(unicode_view_copy(head.content))};
            return result;
        }

        case token_newline:
        case token_space:
        case token_indentation:
            pop(parser);
            break;

        case token_left_parenthesis:
        case token_right_parenthesis:
        case token_colon:
        case token_comma:
        case token_assign:
        case token_fat_arrow:
            pop(parser);
            parser->on_error(parse_error_create(head.where), parser->user);
            break;

        case token_integer:
        {
            pop(parser);
            integer value;
            if (integer_parse(&value, head.content))
            {
                expression_parser_result result = {
                    1, expression_from_integer_literal(value)};
                return result;
            }
            parser->on_error(parse_error_create(head.where), parser->user);
            break;
        }
        }
    }
}

expression_parser_result parse_expression(expression_parser *parser)
{
    expression_parser_result callee = parse_callable(parser);
    if (!callee.is_success)
    {
        return callee;
    }
    expression_parser_result result = callee;
    for (;;)
    {
        rich_token const maybe_open = peek(parser);
        if (maybe_open.token != token_left_parenthesis)
        {
            return result;
        }
        pop(parser);
        expression *arguments = NULL;
        size_t argument_count = 0;
        int expect_another_argument = 0;
        for (;;)
        {
            {
                rich_token const maybe_close = peek(parser);
                if (maybe_close.token == token_right_parenthesis)
                {
                    pop(parser);
                    if (expect_another_argument)
                    {
                        parser->on_error(parse_error_create(maybe_close.where),
                                         parser->user);
                    }
                    result.success = expression_from_call(
                        call_create(expression_allocate(result.success),
                                    tuple_create(arguments, argument_count)));
                    break;
                }
            }
            expression_parser_result const argument = parse_expression(parser);
            if (argument.is_success)
            {
                /*TODO: avoid O(N^2)*/
                arguments = reallocate_array(
                    arguments, argument_count + 1, sizeof(*arguments));
                arguments[argument_count] = argument.success;
                argument_count++;
            }
            rich_token const maybe_comma = peek(parser);
            if (maybe_comma.token == token_comma)
            {
                expect_another_argument = 1;
                pop(parser);
            }
            else
            {
                expect_another_argument = 0;
            }
        }
    }
}
