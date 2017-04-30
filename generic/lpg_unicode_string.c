#include "lpg_unicode_string.h"
#include <string.h>
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include <assert.h>

unicode_string unicode_string_from_range(char const *data, size_t length)
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
    memcpy(result.data, c_str, result.length);
    return result;
}

void unicode_string_free(unicode_string const *s)
{
    deallocate(s->data);
}

bool unicode_string_equals(unicode_string const left,
                           unicode_string const right)
{
    return (left.length == right.length) &&
           !memcmp(left.data, right.data, left.length);
}
