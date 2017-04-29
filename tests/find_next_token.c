#include "find_next_token.h"
#include "test.h"

rich_token find_next_token(callback_user user)
{
    test_parser_user *const parser_user = user;
    if (parser_user->remaining_size == 0)
    {
        rich_token result = rich_token_create(
            tokenize_success, token_space,
            unicode_view_create(parser_user->remaining_input, 0),
            parser_user->current_location);
        return result;
    }
    tokenize_result tokenized =
        tokenize(parser_user->remaining_input, parser_user->remaining_size);
    REQUIRE(tokenized.length >= 1);
    REQUIRE(tokenized.length <= parser_user->remaining_size);
    rich_token result = rich_token_create(
        tokenized.status, tokenized.token,
        unicode_view_create(parser_user->remaining_input, tokenized.length),
        parser_user->current_location);
    if ((tokenized.status == tokenize_success) &&
        (tokenized.token == token_newline))
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

void handle_error(parse_error const error, callback_user user)
{
    test_parser_user *const parser_user = user;
    REQUIRE(parser_user->expected_count >= 1);
    REQUIRE(parse_error_equals(parser_user->expected_errors[0], error));
    --parser_user->expected_count;
    ++parser_user->expected_errors;
}
