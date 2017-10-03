#include "lpg_find_next_token.h"
#include "lpg_assert.h"

rich_token find_next_token(callback_user user)
{
    parser_user *const parser_user = user;
    if (parser_user->remaining_size == 0)
    {
        rich_token const result =
            rich_token_create(tokenize_success, token_space, unicode_view_create(parser_user->remaining_input, 0),
                              parser_user->current_location);
        return result;
    }
    tokenize_result tokenized = tokenize(parser_user->remaining_input, parser_user->remaining_size);
    ASSUME(tokenized.length >= 1);
    ASSUME(tokenized.length <= parser_user->remaining_size);
    rich_token const result = rich_token_create(tokenized.status, tokenized.token,
                                                unicode_view_create(parser_user->remaining_input, tokenized.length),
                                                parser_user->current_location);
    if ((tokenized.status == tokenize_success) && (tokenized.token == token_newline))
    {
        ++parser_user->current_location.line;
        parser_user->current_location.approximate_column = 0;
    }
    else
    {
        parser_user->current_location.approximate_column += tokenized.length;
    }
    parser_user->remaining_input += tokenized.length;
    parser_user->remaining_size -= tokenized.length;
    return result;
}
