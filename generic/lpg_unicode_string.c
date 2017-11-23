#include "lpg_unicode_string.h"
#include <string.h>
#include "lpg_allocate.h"

unicode_string unicode_string_from_range(char const *data, size_t length)
{
    unicode_string const result = {allocate_array(length, sizeof(*data)), length};
    if (result.data && (length > 0))
    {
        memcpy(result.data, data, length * sizeof(*data));
    }
    return result;
}

unicode_string unicode_string_from_c_str(const char *c_str)
{
    unicode_string result;
    result.length = strlen(c_str);
    result.data = (result.length ? allocate_array(result.length, sizeof(*result.data)) : NULL);
    if (result.data)
    {
        memcpy(result.data, c_str, result.length);
    }
    return result;
}

void unicode_string_free(unicode_string const *s)
{
    if (s->data)
    {
        deallocate(s->data);
    }
}

bool unicode_string_equals(unicode_string const left, unicode_string const right)
{
    if (left.length != right.length)
    {
        return false;
    }
    if (left.length == 0)
    {
        return true;
    }
    return !memcmp(left.data, right.data, left.length);
}

char *unicode_string_c_str(unicode_string *value)
{
    value->data = reallocate(value->data, value->length + 1);
    value->data[value->length] = '\0';
    return value->data;
}

unicode_string unicode_string_validate(blob const raw)
{
    unicode_string result = {raw.data, 0};
    for (; result.length < raw.length; ++result.length)
    {
        if ((unsigned char)(raw.data[result.length]) >= 127)
        {
            break;
        }
    }
    return result;
}
