#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "lpg_non_null.h"
#include "lpg_use_result.h"

typedef struct blob
{
    char *data;
    size_t length;
} blob;

blob blob_from_range(char const *data, size_t length) LPG_USE_RESULT;
blob blob_from_c_str(LPG_NON_NULL(const char *c_str)) LPG_USE_RESULT;
void blob_free(LPG_NON_NULL(blob const *s));
bool blob_equals(blob const left, blob const right) LPG_USE_RESULT;
