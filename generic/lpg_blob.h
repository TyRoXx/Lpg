#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct blob
{
    char *data;
    size_t length;
} blob;

blob blob_from_range(char const *data, size_t length);
blob blob_from_c_str(const char *c_str);
void blob_free(blob const *s);
bool blob_equals(blob const left, blob const right);
