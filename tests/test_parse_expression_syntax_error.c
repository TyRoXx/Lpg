#include "test_parse_expression_syntax_error.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_array_size.h"

typedef struct test_parser_user
{
    unicode_code_point const *remaining_input;
    size_t remaining_size;
    parse_error const *expected_errors;
    size_t expected_count;
    source_location current_location;
} test_parser_user;

static rich_token find_next_token(callback_user user)
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
    REQUIRE(tokenized.status == tokenize_success);
    REQUIRE(tokenized.length <= parser_user->remaining_size);
    rich_token result = rich_token_create(
        tokenized.status, tokenized.token,
        unicode_view_create(parser_user->remaining_input, tokenized.length),
        parser_user->current_location);
    if (tokenized.status == tokenize_success)
    {
        if (tokenized.token == token_newline)
        {
            ++parser_user->current_location.line;
            parser_user->current_location.approximate_column = 0;
        }
        else
        {
            parser_user->current_location.approximate_column +=
                tokenized.length;
        }
    }
    parser_user->remaining_input += tokenized.length;
    parser_user->remaining_size -= tokenized.length;
    return result;
}

static continue_flag handle_error(parse_error const error, callback_user user)
{
    test_parser_user *const parser_user = user;
    REQUIRE(parser_user->expected_count >= 1);
    REQUIRE(parse_error_equals(parser_user->expected_errors[0], error));
    --parser_user->expected_count;
    ++parser_user->expected_errors;
    return continue_yes;
}

static void test_syntax_error(parse_error const *expected_errors,
                              size_t const expected_count,
                              expression *const expected, unicode_string input)
{
    test_parser_user user = {input.data, input.length, expected_errors,
                             expected_count, source_location_create(0, 0)};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser);
    if (expected)
    {
        REQUIRE(result.is_success);
        REQUIRE(expression_equals(expected, &result.success));
        expression_free(expected);
        expression_free(&result.success);
        REQUIRE(user.remaining_size == 0);
    }
    else
    {
        REQUIRE(!result.is_success);
    }
    unicode_string_free(&input);
    REQUIRE(user.expected_count == 0);
}

void test_parse_expression_syntax_error(void)
{
    {
        parse_error const expected_error =
            parse_error_create(source_location_create(0, 0));
        expression expected =
            expression_from_identifier(unicode_string_from_c_str("a"));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("=> a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(source_location_create(0, 0)),
            parse_error_create(source_location_create(0, 2))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("=>"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(source_location_create(0, 0)),
            parse_error_create(source_location_create(1, 4))};
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          NULL, unicode_string_from_c_str("=>\n    "));
    }
    {
        parse_error const expected_error =
            parse_error_create(source_location_create(1, 0));
        expression expected =
            expression_from_identifier(unicode_string_from_c_str("a"));
        test_syntax_error(
            &expected_error, 1, &expected, unicode_string_from_c_str("\n=> a"));
    }
    {
        parse_error const expected_error =
            parse_error_create(source_location_create(1, 4));
        expression expected =
            expression_from_identifier(unicode_string_from_c_str("a"));
        test_syntax_error(&expected_error, 1, &expected,
                          unicode_string_from_c_str("\n    => a"));
    }
    {
        parse_error const expected_errors[] = {
            parse_error_create(source_location_create(0, 0)),
            parse_error_create(source_location_create(1, 0)),
            parse_error_create(source_location_create(2, 0))};
        expression expected =
            expression_from_identifier(unicode_string_from_c_str("a"));
        test_syntax_error(expected_errors, LPG_ARRAY_SIZE(expected_errors),
                          &expected, unicode_string_from_c_str("=>\n"
                                                               "=>\n"
                                                               "=> a"));
    }
}
