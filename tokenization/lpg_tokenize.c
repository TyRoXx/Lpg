#include "lpg_tokenize.h"
#include "lpg_assert.h"
#include "lpg_identifier.h"
#include "lpg_stream_writer.h"
#include "lpg_string_literal.h"
#include "lpg_unicode_view.h"
#include <string.h>

static tokenize_result tokenize_space(const char *input, size_t length);
static tokenize_result tokenize_identifier(const char *input, size_t length);
static tokenize_result tokenize_equal_signs(const char *input, size_t length);
static tokenize_result tokenize_less_than(const char *input, size_t length);
static tokenize_result tokenize_less_greater(const char *input, size_t length);
static tokenize_result tokenize_not(const char *input, size_t length);

static tokenize_result make_success(enum token_type token, size_t length)
{
    tokenize_result const result = {tokenize_success, token, length};
    return result;
}

static int is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

static bool is_new_line(char c)
{
    return c == '\n' || c == '\r';
}

static bool is_bracket(char c)
{
    return strchr("(){}[]", c) != NULL;
}

static int can_follow_integer(char c)
{
    ASSUME(!is_digit(c));

    if (is_identifier_middle(c))
    {
        return 0;
    }
    else if (is_bracket(c) || is_new_line(c))
    {
        return 1;
    }

    switch (c)
    {
    case ' ':
    case '=':
    case ':':
    case ',':
    case '.':
        return 1;

    default:
        return 0;
    }
}

static tokenize_result tokenize_single_line_comment(char const *input, size_t length)
{
    size_t comment_length = 1;
    while (comment_length < length)
    {
        if (is_new_line(input[comment_length]))
        {
            return make_success(token_comment, comment_length);
        }
        ++comment_length;
    }
    return make_success(token_comment, comment_length);
}

static tokenize_result tokenize_multi_line_comment(char const *input, size_t length)
{
    size_t comment_length = 1;
    bool asterisk = false;
    while (comment_length < length)
    {
        if (asterisk && input[comment_length] == '/')
        {
            return make_success(token_comment, comment_length + 1);
        }
        asterisk = (input[comment_length] == '*');
        ++comment_length;
    }
    tokenize_result const result = {tokenize_invalid, token_comment, comment_length};
    return result;
}

static tokenize_result tokenize_integer(char const *input, size_t length)
{
    size_t i;
    for (i = 1; (i < length) && is_digit(input[i]); ++i)
    {
    }
    if ((i < length) && !can_follow_integer(input[i]))
    {
        tokenize_result const result = {tokenize_invalid, token_space, i};
        return result;
    }
    return make_success(token_integer, i);
}

static tokenize_result tokenize_string(char const *input, size_t length)
{
    decode_string_literal_result const decoding_result =
        decode_string_literal(unicode_view_create(input, length), stream_writer_create_null_writer());
    if (decoding_result.is_valid)
    {
        return make_success(token_string, decoding_result.length);
    }
    tokenize_result const result = {tokenize_invalid, token_string, decoding_result.length};
    return result;
}

static tokenize_result tokenize_raw_string(char const *input, size_t length)
{
    tokenize_result result = {tokenize_invalid, token_raw_string, length};
    if (length == 1)
    {
        return result;
    }

    size_t string_length;
    for (string_length = 1; string_length < length; ++string_length)
    {
        if (input[string_length] == '\'')
        {
            break;
        }
    }
    if ((string_length < length) && input[string_length] == '\'')
    {
        return make_success(token_raw_string, string_length + 1);
    }
    else
    {
        result.length = string_length;
        return result;
    }
}

tokenize_result tokenize(char const *input, size_t length)
{
    ASSUME(length > 0);
    if (*input == ' ')
    {
        return tokenize_space(input, length);
    }
    if (*input == '\t')
    {
        tokenize_result const result = {tokenize_success, token_indentation, 1};
        return result;
    }
    if (*input == '/')
    {
        if (length == 1)
        {
            tokenize_result const result = {tokenize_invalid, token_comment, 1};
            return result;
        }

        // Single line comment
        if (input[1] == '/')
        {
            return tokenize_single_line_comment(input, length);
        }
        if (input[1] == '*')
        {
            return tokenize_multi_line_comment(input, length);
        }
    }
    if (is_identifier_begin(*input))
    {
        return tokenize_identifier(input, length);
    }
    if (is_digit(*input))
    {
        return tokenize_integer(input, length);
    }
    switch (*input)
    {
    case '\n':
        return make_success(token_newline, 1);

    case '\r':
        if ((length >= 2) && (input[1] == '\n'))
        {
            return make_success(token_newline, 2);
        }
        return make_success(token_newline, 1);

    case '(':
        return make_success(token_left_parenthesis, 1);

    case ')':
        return make_success(token_right_parenthesis, 1);

    case '{':
        return make_success(token_left_curly_brace, 1);

    case '}':
        return make_success(token_right_curly_brace, 1);

    case '[':
        return make_success(token_left_bracket, 1);

    case ']':
        return make_success(token_right_bracket, 1);

    case ':':
        return make_success(token_colon, 1);

    case ',':
        return make_success(token_comma, 1);

    case '.':
        return make_success(token_dot, 1);

    case '=':
        return tokenize_equal_signs(input, length);

    case '!':
        return tokenize_not(input, length);

    case '<':
        return tokenize_less_than(input, length);

    case '>':
        return tokenize_less_greater(input, length);

    case '"':
        return tokenize_string(input, length);

    case '\'':
        return tokenize_raw_string(input, length);

    default:
    {
        tokenize_result const result = {tokenize_invalid, token_space, 1};
        return result;
    }
    }
}

static tokenize_result tokenize_not(const char *input, size_t length)
{
    if (length < 2)
    {
        return make_success(token_not, 1);
    }
    char const second = input[1];
    switch (second)
    {
    case '=':
        return make_success(token_not_equals, 2);
    default:
        return make_success(token_not, 1);
    }
}

static tokenize_result tokenize_less_than(const char *input, size_t length)
{
    if (length < 2)
    {
        return make_success(token_less_than, 1);
    }
    char const second = input[1];
    switch (second)
    {
    case '=':
        return make_success(token_less_than_or_equals, 2);
    default:
        return make_success(token_less_than, 1);
    }
}

static tokenize_result tokenize_less_greater(const char *input, size_t length)
{
    if (length < 2)
    {
        return make_success(token_greater_than, 1);
    }
    char const second = input[1];
    switch (second)
    {
    case '=':
        return make_success(token_greater_than_or_equals, 2);
    default:
        return make_success(token_greater_than, 1);
    }
}

static tokenize_result tokenize_identifier(const char *input, size_t length)
{
    size_t i;

    for (i = 1; (i < length) && is_identifier_middle(input[i]); ++i)
    {
    }

#define LPG_EQUALS_LITERAL(view, literal) (!memcmp((view).begin, (literal), sizeof(literal) - 1))

    unicode_view const content = unicode_view_create(input, i);
    if (content.length <= 5)
    {
        if (content.length == 4)
        {
            if (LPG_EQUALS_LITERAL(content, "case"))
            {
                return make_success(token_case, content.length);
            }
            if (LPG_EQUALS_LITERAL(content, "loop"))
            {
                return make_success(token_loop, content.length);
            }
            if (LPG_EQUALS_LITERAL(content, "impl"))
            {
                return make_success(token_impl, content.length);
            }
            if (LPG_EQUALS_LITERAL(content, "enum"))
            {
                return make_success(token_enum, content.length);
            }
        }
        if (unicode_view_equals_c_str(content, "let"))
        {
            return make_success(token_let, content.length);
        }
        if (content.length == 5)
        {
            if (LPG_EQUALS_LITERAL(content, "break"))
            {
                return make_success(token_break, content.length);
            }
            if (LPG_EQUALS_LITERAL(content, "match"))
            {
                return make_success(token_match, content.length);
            }
        }
    }
    if (content.length == 6)
    {
        if (LPG_EQUALS_LITERAL(content, "struct"))
        {
            return make_success(token_struct, content.length);
        }
        if (LPG_EQUALS_LITERAL(content, "return"))
        {
            return make_success(token_return, content.length);
        }
        if (LPG_EQUALS_LITERAL(content, "import"))
        {
            return make_success(token_import, content.length);
        }
    }
    if (unicode_view_equals_c_str(content, "type_of"))
    {
        return make_success(token_type_of, content.length);
    }
    if (content.length == 9)
    {
        if (LPG_EQUALS_LITERAL(content, "new_array"))
        {
            return make_success(token_new_array, content.length);
        }
        if (LPG_EQUALS_LITERAL(content, "interface"))
        {
            return make_success(token_interface, content.length);
        }
    }

#undef LPG_EQUALS_LITERAL

    return make_success(token_identifier, i);
}

static tokenize_result tokenize_space(const char *input, size_t length)
{
    if (length >= spaces_for_indentation)
    {
        size_t i = 1;
        while ((i < length) && (input[i] == ' '))
        {
            ++i;
        }
        if (i >= spaces_for_indentation)
        {
            return make_success(token_indentation, (i - (i % spaces_for_indentation)));
        }
    }
    return make_success(token_space, 1);
}

static tokenize_result tokenize_equal_signs(const char *input, size_t length)
{
    if (length < 2)
    {
        return make_success(token_assign, 1);
    }
    char const second = input[1];
    switch (second)
    {
    case '>':
        return make_success(token_fat_arrow, 2);
    case '=':
        return make_success(token_equals, 2);
    default:
        return make_success(token_assign, 1);
    }
}
