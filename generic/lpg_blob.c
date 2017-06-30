#include "lpg_blob.h"
#include <string.h>
#include "lpg_allocate.h"

blob blob_from_range(char const *data, size_t length)
{
    blob result = {allocate_array(length, sizeof(*data)), length};
    memcpy(result.data, data, length * sizeof(*data));
    return result;
}

blob blob_from_c_str(const char *c_str)
{
    blob result;
    result.length = strlen(c_str);
    result.data = allocate_array(result.length, sizeof(*result.data));
    memcpy(result.data, c_str, result.length);
    return result;
}

void blob_free(blob const *s)
{
    if (s->data)
    {
        deallocate(s->data);
    }
}

bool blob_equals(blob const left, blob const right)
{
    return (left.length == right.length) &&
           !memcmp(left.data, right.data, left.length);
}
