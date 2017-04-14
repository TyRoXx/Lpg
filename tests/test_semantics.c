#include "test_semantics.h"
#include "test.h"
#include "lpg_check.h"
#include "lpg_parse_expression.h"
#include <string.h>

typedef struct test_parser_user
{
    char const *remaining_input;
    size_t remaining_size;
} test_parser_user;

static rich_token find_next_token(callback_user user)
{
    test_parser_user *const parser_user = user;
    if (parser_user->remaining_size == 0)
    {
        return rich_token_create(tokenize_success, token_space,
                                 unicode_view_create(NULL, 0),
                                 source_location_create(0, 0));
    }
    tokenize_result tokenized =
        tokenize(parser_user->remaining_input, parser_user->remaining_size);
    REQUIRE(tokenized.status == tokenize_success);
    REQUIRE(tokenized.length <= parser_user->remaining_size);
    rich_token result = rich_token_create(
        tokenize_success, tokenized.token,
        unicode_view_create(parser_user->remaining_input, tokenized.length),
        source_location_create(0, 0));
    parser_user->remaining_input += tokenized.length;
    parser_user->remaining_size -= tokenized.length;
    return result;
}

static void handle_error(parse_error const error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static sequence parse(char const *input)
{
    test_parser_user user = {input, strlen(input)};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.remaining_size == 0);
    return result;
}

void test_semantics(void)
{
    {
        sequence root = sequence_create(NULL, 0);
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].size == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("f()");
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].size == 1);
        REQUIRE(checked.functions[0].body[0].type == instruction_call);
        checked_program_free(&checked);
    }
}
