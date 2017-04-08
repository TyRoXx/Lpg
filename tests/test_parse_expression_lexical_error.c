#include "test_parse_expression_lexical_error.h"
#include "test.h"
#include "lpg_parse_expression.h"

typedef struct test_parser_user
{
    unicode_code_point const *remaining_input;
    size_t remaining_size;
} test_parser_user;

static rich_token find_next_token(callback_user user)
{
    test_parser_user *const parser_user = user;
    tokenize_result tokenized =
        tokenize(parser_user->remaining_input, parser_user->remaining_size);
    REQUIRE(tokenized.length <= parser_user->remaining_size);
    rich_token result = {tokenized.status, tokenized.token,
                         parser_user->remaining_input, tokenized.length};
    parser_user->remaining_input += tokenized.length;
    parser_user->remaining_size -= tokenized.length;
    return result;
}

static continue_flag handle_error(parse_error const error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void test_lexical_error(unicode_string input)
{
    test_parser_user user = {input.data, input.length};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser);
    REQUIRE(!result.is_success);
    REQUIRE(user.remaining_size != 0);
    unicode_string_free(&input);
}

void test_parse_expression_lexical_error(void)
{
    test_lexical_error(unicode_string_from_c_str("?"));
    test_lexical_error(unicode_string_from_c_str("?a"));
    test_lexical_error(unicode_string_from_c_str("?break"));
}
