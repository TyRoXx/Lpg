#include "lpg_tokenize.h"
#include "lpg_assert.h"
#include "lpg_identifier.h"

static tokenize_result make_success(enum token_type token, size_t length)
{
    tokenize_result result = {tokenize_success, token, length};
    return result;
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
    switch (*input)
    {
    case '\n':
        return make_success(token_newline, 1);
    case '(':
    case ')':
    case ':':
        return make_success(token_operator, 1);
    }
    tokenize_result result = {tokenize_invalid, token_space, 0};
    return result;
}
