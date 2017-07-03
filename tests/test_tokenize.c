#include "test_tokenize.h"
#include "test.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_string.h"

static void test_single_token(token_type expected_token, size_t expected_size,
                              char const *input)
{
    unicode_string s = unicode_string_from_c_str(input);
    tokenize_result const result = tokenize(s.data, s.length);
    unicode_string_free(&s);
    REQUIRE(result.status == tokenize_success);
    REQUIRE(result.length == expected_size);
    REQUIRE(result.token == expected_token);
}

static void test_invalid(char const *input)
{
    unicode_string s = unicode_string_from_c_str(input);
    tokenize_result const result = tokenize(s.data, s.length);
    unicode_string_free(&s);
    REQUIRE(result.status == tokenize_invalid);
}

void test_tokenize(void)
{
    test_single_token(token_newline, 1, "\n");
    test_single_token(token_newline, 2, "\r\n");
    test_single_token(token_newline, 2, "\r\nabc");
    test_single_token(token_newline, 1, "\rabc");
    test_single_token(token_newline, 1, "\n\n");
    test_single_token(token_newline, 1, "\n ");
    test_single_token(token_newline, 1, "\naaaa");
    test_single_token(token_space, 1, " ");
    test_single_token(token_space, 1, "  ");
    test_single_token(token_space, 1, "   ");
    test_single_token(token_space, 1, " abc");
    test_single_token(token_space, 1, " ?\n");
    test_single_token(token_indentation, 4, "    ");
    test_single_token(token_indentation, 4, "    a");
    test_single_token(token_indentation, 8, "         ");
    test_single_token(token_indentation, 12, "             ");
    test_single_token(token_identifier, 1, "a");
    test_single_token(token_identifier, 1, "a ");
    test_single_token(token_identifier, 2, "ab");
    test_single_token(token_identifier, 1, "a?");
    test_single_token(token_identifier, 1, "a b ");
    test_single_token(token_identifier, 2, "a- b ");
    test_single_token(token_identifier, 3, "a-b b ");
    test_single_token(token_left_parenthesis, 1, "(");
    test_single_token(token_right_parenthesis, 1, ")");
    test_single_token(token_colon, 1, ":");
    test_single_token(token_colon, 1, ":a");
    test_single_token(token_colon, 1, "::");
    test_single_token(token_assign, 1, "=");
    test_single_token(token_assign, 1, "==");
    test_single_token(token_fat_arrow, 2, "=>");
    test_single_token(token_comma, 1, ",");
    test_single_token(token_dot, 1, ".");
    test_single_token(token_dot, 1, "..");
    test_single_token(token_integer, 1, "1");
    test_single_token(token_integer, 1, "1)");
    test_single_token(token_integer, 1, "1 a");
    test_single_token(token_integer, 2, "12");
    test_single_token(token_integer, 3, "123 456");
    test_single_token(token_break, 5, "break");
    test_single_token(token_case, 4, "case");
    test_single_token(token_loop, 4, "loop");
    test_single_token(token_return, 6, "return");
    test_single_token(token_match, 5, "match");
    test_single_token(token_let, 3, "let");
    test_single_token(token_string, 2, "\"\"");
    test_single_token(token_string, 3, "\"a\"");
    test_single_token(token_string, 5, "\"abc\"");
    test_single_token(token_string, 6, "\"a\\nc\"");
    test_single_token(token_comment, 13, "//Hello World");
    test_single_token(token_comment, 13, "//Hello World\nTest");
    test_single_token(token_comment, 15, "/*Hello World*/");
    test_single_token(token_comment, 15, "/*Hello World*/testings");
    test_invalid("?");
    test_invalid("\t");
    test_invalid("'aaa");
    test_invalid("-a");
    test_invalid("1a");
    test_invalid("\"");
    test_invalid("\"a");
    test_invalid("\"a\nc\"");
    test_invalid("\"\\\"");
    test_invalid("/*Hello World");
    test_invalid("/*Hello World*");
    test_invalid("/**");
}
