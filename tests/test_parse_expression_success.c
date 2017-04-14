#include "test_parse_expression_success.h"
#include "test.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"

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

static void test_successful_parse(expression expected, unicode_string input)
{
    test_parser_user user = {input.data, input.length};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    expression_parser_result result = parse_expression(&parser, 0, 1);
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
    {
        expression *elements = allocate_array(1, sizeof(*elements));
        elements[0] = expression_from_break();
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 1)),
            unicode_string_from_c_str("loop\n"
                                      "    break"));
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] =
            expression_from_identifier(unicode_string_from_c_str("f"));
        elements[1] = expression_from_break();
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    f\n"
                                      "    break"));
    }
    {
        expression *elements = allocate_array(2, sizeof(*elements));
        elements[0] =
            expression_from_identifier(unicode_string_from_c_str("f"));
        elements[1] = expression_from_break();
        test_successful_parse(
            expression_from_loop(sequence_create(elements, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    f\n"
                                      "    break\n"));
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break();
        expression *outer_loop = allocate_array(1, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1));
        test_successful_parse(
            expression_from_loop(sequence_create(outer_loop, 1)),
            unicode_string_from_c_str("loop\n"
                                      "    loop\n"
                                      "        break\n"));
    }
    {
        expression *inner_loop = allocate_array(1, sizeof(*inner_loop));
        inner_loop[0] = expression_from_break();
        expression *outer_loop = allocate_array(2, sizeof(*outer_loop));
        outer_loop[0] = expression_from_loop(sequence_create(inner_loop, 1));
        outer_loop[1] = expression_from_break();
        test_successful_parse(
            expression_from_loop(sequence_create(outer_loop, 2)),
            unicode_string_from_c_str("loop\n"
                                      "    loop\n"
                                      "        break\n"
                                      "    break\n"));
    }

    test_successful_parse(
        expression_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))))),
        unicode_string_from_c_str("a = 1"));

    test_successful_parse(
        expression_from_declare(declare_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("int"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 1))))),
        unicode_string_from_c_str("a : int = 1"));
}
