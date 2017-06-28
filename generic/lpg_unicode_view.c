#include "lpg_unicode_view.h"
#include <string.h>
#include "lpg_assert.h"

unicode_view unicode_view_create(char const *begin, size_t length)
{
    unicode_view result = {begin, length};
    return result;
}

unicode_view unicode_view_from_c_str(char const *c_str)
{
    return unicode_view_create(c_str, strlen(c_str));
}

unicode_view unicode_view_from_string(unicode_string string)
{
    unicode_view result = {string.data, string.length};
    return result;
}

bool unicode_view_equals_c_str(unicode_view left, char const *right)
{
    size_t const length = strlen(right);
    if (left.length != length)
    {
        return 0;
    }
    return !memcmp(left.begin, right, length);
}

bool unicode_view_equals(unicode_view left, unicode_view right)
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

unicode_view unicode_view_cut(unicode_view const whole, size_t const begin,
                              size_t const end)
{
    ASSUME(begin <= end);
    ASSUME(end <= whole.length);
    return unicode_view_create(whole.begin + begin, (end - begin));
}
