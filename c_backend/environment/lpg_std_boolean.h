#pragma once
#include <stddef.h>
static size_t and_impl(size_t const left, size_t const right)
{
    return (left && right);
}

static size_t or_impl(size_t const left, size_t const right)
{
    return (left || right);
}

static size_t not_impl(size_t const argument)
{
    return !argument;
}
