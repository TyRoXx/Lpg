#pragma once
#include "lpg_blob.h"

typedef struct blob_or_error
{
    char const *error;
    blob success;
} blob_or_error;

blob_or_error make_blob_success(blob success) LPG_USE_RESULT;

blob_or_error make_blob_error(LPG_NON_NULL(char const *const error)) LPG_USE_RESULT;

blob_or_error read_file(LPG_NON_NULL(char const *const name)) LPG_USE_RESULT;
