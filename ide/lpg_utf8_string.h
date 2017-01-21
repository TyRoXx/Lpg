#pragma once
#include "lpg_allocate.h"
#include <string.h>

typedef struct utf8_string
{
    char *data;
    size_t length;
} utf8_string;

static utf8_string utf8_string_from_c_str(const char *c_str)
{
    utf8_string result;
    result.length = strlen(c_str);
    result.data = allocate(result.length);
    memcpy(result.data, c_str, result.length);
    return result;
}

static void utf8_string_insert(utf8_string *const s, size_t const position,
                               unicode_code_point const inserted)
{
    assert(position <= s->length);
    if (inserted > 0x9F)
    {
        abort();
    }
    char *const new_data = allocate(s->length + 1);
    memcpy(new_data, s->data, position);
    new_data[position] = (char)inserted;
    memcpy(new_data + position + 1, s->data + position, s->length - position);
    deallocate(s->data);
    s->data = new_data;
    s->length++;
}

static void utf8_string_erase(utf8_string *const s, size_t const position)
{
    assert(position < s->length);
    memmove(
        s->data + position, s->data + position + 1, s->length - position - 1);
    --s->length;
}

static void utf8_string_free(utf8_string *s)
{
    deallocate(s->data);
}
