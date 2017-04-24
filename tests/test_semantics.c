#include "test_semantics.h"
#include "test.h"
#include "lpg_check.h"
#include "lpg_parse_expression.h"
#include "lpg_allocate.h"
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
    REQUIRE(!instruction_equals(
        instruction_create_loop(instruction_sequence_create(NULL, 0)),
        instruction_create_call()));
    {
        instruction_sequence const left = instruction_sequence_create(NULL, 0);
        instruction_sequence const right = instruction_sequence_create(
            allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] = instruction_create_call();
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        instruction_sequence const left = instruction_sequence_create(
            allocate_array(1, sizeof(*left.elements)), 1);
        left.elements[0] =
            instruction_create_loop(instruction_sequence_create(NULL, 0));
        instruction_sequence const right = instruction_sequence_create(
            allocate_array(1, sizeof(*right.elements)), 1);
        right.elements[0] = instruction_create_call();
        REQUIRE(!instruction_sequence_equals(&left, &right));
        instruction_sequence_free(&left);
        instruction_sequence_free(&right);
    }
    {
        sequence root = sequence_create(NULL, 0);
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        REQUIRE(checked.functions[0].body.length == 0);
        checked_program_free(&checked);
    }
    {
        sequence root = parse("f()");
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] = instruction_create_call();
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("loop\n"
                              "    f()");
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const loop_body = allocate_array(1, sizeof(*loop_body));
        loop_body[0] = instruction_create_call();
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(instruction_sequence_create(loop_body, 1));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
    {
        sequence root = parse("loop\n"
                              "    f()"
                              "    g()");
        checked_program checked = check(root);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        instruction *const loop_body = allocate_array(2, sizeof(*loop_body));
        loop_body[0] = instruction_create_call();
        loop_body[1] = instruction_create_call();
        instruction *const expected_body_elements =
            allocate_array(1, sizeof(*expected_body_elements));
        expected_body_elements[0] =
            instruction_create_loop(instruction_sequence_create(loop_body, 2));
        instruction_sequence const expected_body =
            instruction_sequence_create(expected_body_elements, 1);
        REQUIRE(instruction_sequence_equals(
            &expected_body, &checked.functions[0].body));
        checked_program_free(&checked);
        instruction_sequence_free(&expected_body);
    }
}
