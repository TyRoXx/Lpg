#include "lpg_unicode_view.h"
#include <string.h>

unicode_view unicode_view_create(unicode_code_point const *begin, size_t length)
{
    unicode_view result = {begin, length};
    return result;
}

unicode_view unicode_view_from_string(unicode_string string)
{
    unicode_view result = {string.data, string.length};
    return result;
}

int unicode_view_equals_c_str(unicode_view left, char const *right)
{
    size_t i;
    for (i = 0; (i < left.length) && (right[i] != '\0'); ++i)
    {
        /*TODO implement UTF-8*/
        if (left.begin[i] != (unsigned char)right[i])
        {
            return 0;
        }
    }
    return (i == left.length) && (right[i] == '\0');
}

int unicode_view_equals(unicode_view left, unicode_view right)
{
    if (left.length != right.length)
    {
        return 0;
    }
    return !memcmp(left.begin, right.begin, left.length * sizeof(*left.begin));
}

unicode_string unicode_view_copy(unicode_view value)
{
    return unicode_string_from_range(value.begin, value.length);
}
