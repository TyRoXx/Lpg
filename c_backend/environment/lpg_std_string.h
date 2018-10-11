#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct string
{
    char const *data;
    size_t length;
    size_t *references;
} string;

static string string_create(char const *const data, size_t const length, size_t *const references)
{
    string const result = {data, length, references};
    return result;
}

static string string_literal(char const *const data, size_t const length)
{
    return string_create(data, length, NULL);
}

static void string_add_reference(string const *const s)
{
    if (!s->references)
    {
        return;
    }
    (*s->references) += 1;
}

static void string_free(string const *const s)
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

static bool string_equals(string const left, string const right)
{
    if (left.length != right.length)
    {
        return false;
    }
    return !memcmp(left.data, right.data, left.length);
}

static string string_concat(string const left, string const right)
{
    size_t const result_length = (left.length + right.length);
    char *const allocation = malloc(sizeof(size_t) + result_length);
    if (!allocation)
    {
        abort();
    }
    size_t *const references = (size_t *)allocation;
    *references = 1;
    string const result = {allocation + sizeof(*references), result_length, references};
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
