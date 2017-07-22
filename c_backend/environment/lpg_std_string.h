#pragma once
#include "lpg_std_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

typedef struct string_ref
{
    char const *data;
    size_t begin;
    size_t length;
    size_t *references;
} string_ref;

static string_ref string_ref_create(char const *const data, size_t const begin,
                                    size_t const length,
                                    size_t *const references)
{
    string_ref result = {data, begin, length, references};
    return result;
}

static string_ref string_literal(char const *const data, size_t const length)
{
    return string_ref_create(data, 0, length, NULL);
}

static void string_ref_add_reference(string_ref const *const s)
{
    if (!s->references)
    {
        return;
    }
    (*s->references) += 1;
}

static void string_ref_free(string_ref const *const s)
{
    if (!s->references)
    {
        return;
    }
    assert((*s->references) > 0);
    (*s->references) -= 1;
    if (*s->references > 0)
    {
        return;
    }
    free((char *)s->data);
    free(s->references);
}

static bool string_ref_equals(string_ref const left, string_ref const right)
{
    if (left.length != right.length)
    {
        return false;
    }
    return !memcmp(
        left.data + left.begin, right.data + right.begin, left.length);
}

static string_ref string_ref_concat(string_ref const left,
                                    string_ref const right)
{
    size_t const result_length = (left.length + right.length);
    size_t *const references = malloc(sizeof(*references));
    *references = 1;
    string_ref const result = {
        malloc(result_length), 0, result_length, references};
    if (left.data)
    {
        memcpy((char *)result.data, left.data + left.begin, left.length);
    }
    if (right.data)
    {
        memcpy((char *)result.data + left.length, right.data + right.begin,
               right.length);
    }
    return result;
}
