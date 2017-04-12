#include "test_parse_expression_success.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"

typedef struct test_parser_user
{
    unicode_code_point const *remaining_input;
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

static continue_flag handle_error(parse_error const error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void test_successful_parse(expression expected, unicode_string input)
{
    test_parser_user user = {input.data, input.length};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser);
    REQUIRE(result.is_success);
    REQUIRE(user.remaining_size == 0);
    REQUIRE(expression_equals(&expected, &result.success));
    expression_free(&expected);
    expression_free(&result.success);
    unicode_string_free(&input);
}

void test_parse_expression_success(void)
{
    test_successful_parse(
        expression_from_break(), unicode_string_from_c_str("break"));
    test_successful_parse(
        expression_from_identifier(unicode_string_from_c_str("a")),
        unicode_string_from_c_str("a"));
    test_successful_parse(
        expression_from_integer_literal(integer_create(0, 123)),
        unicode_string_from_c_str("123"));
    {
        tuple arguments = tuple_create(NULL, 0);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("f"))),
                arguments)),
            unicode_string_from_c_str("f()"));
    }
    {
        tuple arguments = tuple_create(NULL, 0);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(expression_from_call(
                    call_create(expression_allocate(expression_from_identifier(
                                    unicode_string_from_c_str("f"))),
                                arguments))),
                arguments)),
            unicode_string_from_c_str("f()()"));
    }
    {
        expression *arguments = allocate_array(1, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        tuple arguments_tuple = tuple_create(arguments, 1);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("f"))),
                arguments_tuple)),
            unicode_string_from_c_str("f(1)"));
    }
    {
        expression *arguments = allocate_array(2, sizeof(*arguments));
        arguments[0] = expression_from_integer_literal(integer_create(0, 1));
        arguments[1] = expression_from_integer_literal(integer_create(0, 2));
        tuple arguments_tuple = tuple_create(arguments, 2);
        test_successful_parse(
            expression_from_call(call_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("f"))),
                arguments_tuple)),
            unicode_string_from_c_str("f(1,2)"));
    }
}
