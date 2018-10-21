#include "lpg_find_next_token.h"
#include "lpg_assert.h"

static rich_token rich_token_create(tokenize_status status, token_type token, unicode_view content, source_location where)
{
    rich_token const result = {status, token, content, where};
    return result;
}

rich_token find_next_token(callback_user user)
{
    parser_user *const actual_user = user;
    if (actual_user->remaining_size == 0)
    {
        rich_token const result =
            rich_token_create(tokenize_success, token_space, unicode_view_create(actual_user->remaining_input, 0),
                              actual_user->current_location);
        return result;
    }
    tokenize_result tokenized = tokenize(actual_user->remaining_input, actual_user->remaining_size);
    ASSUME(tokenized.length >= 1);
    ASSUME(tokenized.length <= actual_user->remaining_size);
    rich_token const result = rich_token_create(tokenized.status, tokenized.token,
                                                unicode_view_create(actual_user->remaining_input, tokenized.length),
                                                actual_user->current_location);
    if ((tokenized.status == tokenize_success) && (tokenized.token == token_newline))
    {
        ++actual_user->current_location.line;
        actual_user->current_location.approximate_column = 0;
    }
    else
    {
        actual_user->current_location.approximate_column += tokenized.length;
    }
    actual_user->remaining_input += tokenized.length;
    actual_user->remaining_size -= tokenized.length;
    return result;
}
