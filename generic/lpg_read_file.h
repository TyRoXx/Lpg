#pragma once
#include "lpg_blob.h"
#include "lpg_unicode_view.h"

typedef struct blob_or_error
{
    char const *error;
    blob success;
} blob_or_error;

blob_or_error make_blob_success(blob success) LPG_USE_RESULT;

blob_or_error make_blob_error(LPG_NON_NULL(char const *const error)) LPG_USE_RESULT;

blob_or_error read_file(LPG_NON_NULL(char const *const name)) LPG_USE_RESULT;

blob_or_error read_file_unicode_view_name(unicode_view const name) LPG_USE_RESULT;

size_t remove_carriage_returns(char *const from, size_t const length);
