#include "lpg_unicode_view.h"
#include <string.h>
#include "lpg_assert.h"
#include "lpg_allocate.h"

unicode_view unicode_view_create(char const *begin, size_t length)
{
    unicode_view const result = {begin, length};
    return result;
}

unicode_view unicode_view_from_c_str(char const *c_str)
{
    return unicode_view_create(c_str, strlen(c_str));
}

unicode_view unicode_view_from_string(unicode_string string)
{
    unicode_view const result = {string.data, string.length};
    return result;
}

bool unicode_view_equals_c_str(unicode_view left, char const *right)
{
    size_t const length = strlen(right);
    if (left.length != length)
    {
        return false;
    }
    return !memcmp(left.begin, right, length);
}

bool unicode_view_equals(unicode_view left, unicode_view right)
{
    if (left.length != right.length)
    {
        return false;
    }
    if (left.length == 0)
    {
        // memcmp must not be called with NULL
        return true;
    }
    return !memcmp(left.begin, right.begin, left.length * sizeof(*left.begin));
}

bool unicode_view_less(unicode_view const left, unicode_view const right)
{
    if (left.length == right.length)
    {
        for (size_t i = 0; i < left.length; ++i)
        {
            if (left.begin[i] < right.begin[i])
            {
                return true;
            }
            if (left.begin[i] > right.begin[i])
            {
                return false;
            }
        }
        return false;
    }
    return left.length < right.length;
}

unicode_string unicode_view_copy(unicode_view value)
{
    return unicode_string_from_range(value.begin, value.length);
}

unicode_view unicode_view_cut(unicode_view const whole, size_t const begin, size_t const end)
{
    ASSUME(begin <= end);
    ASSUME(end <= whole.length);
    return unicode_view_create(whole.begin + begin, (end - begin));
}

optional_size unicode_view_find(unicode_view haystack, const char needle)
{
    for (size_t i = 0; i < haystack.length; ++i)
    {
        if (haystack.begin[i] == needle)
        {
            return make_optional_size(i);
        }
    }
    return optional_size_empty;
}

unicode_string unicode_view_zero_terminate(unicode_view original)
{
    unicode_string const result = {allocate_array(original.length + 1, sizeof(*result.data)), original.length};
    memcpy(result.data, original.begin, result.length);
    result.data[result.length] = '\0';
    return result;
}
