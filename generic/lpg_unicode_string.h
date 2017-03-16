#pragma once
#include "lpg_allocate.h"
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "lpg_assert.h"

typedef uint32_t unicode_code_point;

typedef struct unicode_string
{
    unicode_code_point *data;
    size_t length;
} unicode_string;

static unicode_string unicode_string_from_c_str(const char *c_str)
{
    unicode_string result;
    result.length = strlen(c_str);
    result.data = allocate_array(result.length, sizeof(*result.data));
    for (size_t i = 0; i < result.length; ++i)
    {
        result.data[i] = (unsigned char)c_str[i];
        ASSUME(result.data[i] <= 127);
    }
    return result;
}

static void unicode_string_insert(unicode_string *const s,
                                  size_t const position,
                                  unicode_code_point const inserted)
{
    assert(position <= s->length);
    unicode_code_point *const new_data =
        allocate_array((s->length + 1), sizeof(*new_data));
    memcpy(new_data, s->data, position * sizeof(*new_data));
    new_data[position] = inserted;
    memcpy(new_data + position + 1, s->data + position,
           (s->length - position) * sizeof(*new_data));
    deallocate(s->data);
    s->data = new_data;
    s->length++;
}

static void unicode_string_erase(unicode_string *const s, size_t const position)
{
    assert(position < s->length);
    memmove(s->data + position, s->data + position + 1,
            (s->length - position - 1) * sizeof(*s->data));
    --s->length;
}

static void unicode_string_free(unicode_string *s)
{
    deallocate(s->data);
}
