#pragma once
#include <stddef.h>
#include "lpg_unicode_string.h"

static size_t const spaces_for_indentation = 4;

typedef enum token_type
{
    token_newline,
    token_space,
    token_indentation,
    token_identifier,
    token_operator
} token_type;

typedef enum tokenize_status
{
    tokenize_success,
    tokenize_invalid
} tokenize_status;

typedef struct tokenize_result
{
    tokenize_status status;
    token_type token;
    size_t length;
} tokenize_result;

tokenize_result tokenize(unicode_code_point const *input, size_t length);
