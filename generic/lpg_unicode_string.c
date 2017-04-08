#include "lpg_unicode_string.h"
#include <string.h>
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include <assert.h>

unicode_string unicode_string_from_range(unicode_code_point const *data,
                                         size_t length)
{
    unicode_string result = {allocate_array(length, sizeof(*data)), length};
    memcpy(result.data, data, length * sizeof(*data));
    return result;
}

unicode_string unicode_string_from_c_str(const char *c_str)
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

void unicode_string_insert(unicode_string *const s, size_t const position,
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

void unicode_string_erase(unicode_string *const s, size_t const position)
{
    assert(position < s->length);
    memmove(s->data + position, s->data + position + 1,
            (s->length - position - 1) * sizeof(*s->data));
    --s->length;
}

void unicode_string_free(unicode_string const *s)
{
    deallocate(s->data);
}
