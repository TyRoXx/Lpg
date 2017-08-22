#pragma once
#include "lpg_std_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

typedef struct string_ref
{
    char const *data;
    size_t length;
    size_t *references;
} string_ref;

static string_ref string_ref_create(char const *const data, size_t const length,
                                    size_t *const references)
{
    string_ref result = {data, length, references};
    return result;
}

static string_ref string_literal(char const *const data, size_t const length)
{
    return string_ref_create(data, length, NULL);
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
    free(s->references);
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
    char *const allocation = malloc(sizeof(size_t) + result_length);
    if (!allocation)
    {
        abort();
    }
    size_t *const references = (size_t *)allocation;
    *references = 1;
    string_ref const result = {
        allocation + sizeof(*references), result_length, references};
    if (left.data)
    {
        memcpy((char *)result.data, left.data, left.length);
    }
    if (right.data)
    {
        memcpy((char *)result.data + left.length, right.data, right.length);
    }
    return result;
}
