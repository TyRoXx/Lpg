#include "lpg_tokenize.h"
#include "lpg_assert.h"
#include "lpg_identifier.h"
#include "lpg_unicode_view.h"

static tokenize_result make_success(enum token_type token, size_t length)
{
    tokenize_result result = {tokenize_success, token, length};
    return result;
}

static int is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

static int can_follow_integer(char c)
{
    ASSUME(!is_digit(c));
    if (is_identifier_middle(c))
    {
        return 0;
    }
    switch (c)
    {
    case ' ':
    case '\n':
    case '(':
    case ')':
    case '=':
    case ':':
    case ',':
        return 1;

    default:
        return 0;
    }
}

tokenize_result tokenize(char const *input, size_t length)
{
    ASSUME(length > 0);
    if (*input == ' ')
    {
        if (length >= spaces_for_indentation)
        {
            size_t i = 1;
            while ((i < length) && (input[i] == ' '))
            {
                ++i;
            }
            if (i < spaces_for_indentation)
            {
                return make_success(token_space, 1);
            }
            return make_success(
                token_indentation, (i - (i % spaces_for_indentation)));
        }
        return make_success(token_space, 1);
    }
    if (is_identifier_begin(*input))
    {
        size_t i;
        for (i = 1; (i < length) && is_identifier_middle(input[i]); ++i)
        {
        }
        unicode_view const content = unicode_view_create(input, i);
        if (unicode_view_equals_c_str(content, "case"))
        {
            return make_success(token_case, content.length);
        }
        if (unicode_view_equals_c_str(content, "break"))
        {
            return make_success(token_break, content.length);
        }
        if (unicode_view_equals_c_str(content, "loop"))
        {
            return make_success(token_loop, content.length);
        }
        if (unicode_view_equals_c_str(content, "match"))
        {
            return make_success(token_match, content.length);
        }
        if (unicode_view_equals_c_str(content, "return"))
        {
            return make_success(token_return, content.length);
        }
        return make_success(token_identifier, i);
    }
    if (is_digit(*input))
    {
        size_t i;
        for (i = 1; (i < length) && is_digit(input[i]); ++i)
        {
        }
        if ((i < length) && !can_follow_integer(input[i]))
        {
            tokenize_result result = {tokenize_invalid, token_space, i};
            return result;
        }
        return make_success(token_integer, i);
    }
    switch (*input)
    {
    case '\n':
        return make_success(token_newline, 1);

    case '(':
        return make_success(token_left_parenthesis, 1);

    case ')':
        return make_success(token_right_parenthesis, 1);

    case ':':
        return make_success(token_colon, 1);

    case ',':
        return make_success(token_comma, 1);

    case '=':
        if ((length >= 2) && (input[1] == '>'))
        {
            return make_success(token_fat_arrow, 2);
        }
        return make_success(token_assign, 1);

    case '"':
    {
        size_t i;
        for (i = 1;; ++i)
        {
            if (i == length)
            {
                tokenize_result result = {tokenize_invalid, token_space, i};
                return result;
            }
            if (input[i] == '"')
            {
                ++i;
                break;
            }
        }
        return make_success(token_string, i);
    }
    }
    tokenize_result result = {tokenize_invalid, token_space, 1};
    return result;
}
