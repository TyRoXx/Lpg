#include "lpg_expression_parser.h"
#include "lpg_allocate.h"
#include "lpg_array_size.h"
#include "lpg_assert.h"

expression_parser expression_parser_create(callback_user find_next_token_user, parse_error_handler on_error,
                                           callback_user on_error_user, struct expression_pool *const pool)
{
    expression_parser const result = {find_next_token_user, on_error, on_error_user, NULL, 0, 0, 0, pool};
    return result;
}

void expression_parser_free(expression_parser const freed)
{
    if (freed.cached_tokens)
    {
        deallocate(freed.cached_tokens);
    }
}

parse_error parse_error_create(parse_error_type type, source_location where)
{
    parse_error const result = {type, where};
    return result;
}

bool parse_error_equals(parse_error left, parse_error right)
{
    return (left.type == right.type) && source_location_equals(left.where, right.where);
}

bool expression_parser_has_remaining_non_empty_tokens(expression_parser const *const parser)
{
    return (parser->cached_token_count > 0) && (parser->cached_tokens[0].content.length > 0);
}

rich_token peek_at(expression_parser *parser, size_t const offset)
{
    while (parser->cached_token_count <= offset)
    {
        parser->cached_tokens =
            reallocate_array_exponentially(parser->cached_tokens, (offset + 1), sizeof(*parser->cached_tokens),
                                           parser->cached_token_count, &parser->cached_tokens_allocated);
        parser->cached_tokens[parser->cached_token_count] = find_next_token(parser->find_next_token_user);
        switch (parser->cached_tokens[parser->cached_token_count].status)
        {
        case tokenize_success:
            parser->cached_token_count += 1;
            break;

        case tokenize_invalid:
            parser->on_error(
                parse_error_create(parse_error_invalid_token, parser->cached_tokens[parser->cached_token_count].where),
                parser->on_error_user);
            break;
        }
    }
    ASSUME(parser->cached_tokens[offset].status == tokenize_success);
    return parser->cached_tokens[offset];
}

void pop_n(expression_parser *parser, size_t const count)
{
    ASSUME(parser->cached_token_count >= count);
    parser->cached_token_count -= count;
    memmove(parser->cached_tokens, parser->cached_tokens + count,
            (sizeof(*parser->cached_tokens) * parser->cached_token_count));
}
