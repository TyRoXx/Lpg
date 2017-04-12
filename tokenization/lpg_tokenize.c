#include "lpg_tokenize.h"
#include "lpg_assert.h"
#include "lpg_identifier.h"

static tokenize_result make_success(enum token_type token, size_t length)
{
    tokenize_result result = {tokenize_success, token, length};
    return result;
}

static int is_digit(unicode_code_point c)
{
    return (c >= '0') && (c <= '9');
}

static int can_follow_integer(unicode_code_point c)
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

tokenize_result tokenize(unicode_code_point const *input, size_t length)
{
    ASSUME(length > 0);
    if (*input == ' ')
    {
        if (length >= spaces_for_indentation)
        {
            for (size_t i = 1; i < spaces_for_indentation; ++i)
            {
                if (input[i] != ' ')
                {
                    return make_success(token_space, 1);
                }
            }
            return make_success(token_indentation, spaces_for_indentation);
        }
        return make_success(token_space, 1);
    }
    if (is_identifier_begin(*input))
    {
        size_t i;
        for (i = 1; (i < length) && is_identifier_middle(input[i]); ++i)
        {
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
    }
    tokenize_result result = {tokenize_invalid, token_space, 1};
    return result;
}
