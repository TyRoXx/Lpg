#pragma once
#include "lpg_std_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
typedef struct string_ref
{
    char *data;
    size_t length;
} string_ref;
static string_ref string_ref_create(char *data, size_t length)
{
    string_ref result = {data, length};
    return result;
}
static void string_ref_free(string_ref const *s)
{
    free(s->data);
}
static bool string_ref_equals(string_ref const left, string_ref const right)
{
    if (left.length != right.length)
    {
        return false;
    }
    return !memcmp(left.data, right.data, left.length);
}
static string_ref string_ref_concat(string_ref const left,
                                    string_ref const right)
{
    size_t const result_length = (left.length + right.length);
    string_ref const result = {malloc(result_length), result_length};
    if (left.data)
    {
        memcpy(result.data, left.data, left.length);
    }
    if (right.data)
    {
        memcpy(result.data + left.length, right.data, right.length);
    }
    return result;
}
