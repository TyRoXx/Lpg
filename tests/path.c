#include "print_instruction.h"
#include "path.h"
#include "lpg_allocate.h"
#include <string.h>

unicode_view path_remove_leaf(unicode_view const full)
{
    char const *i = (full.begin + full.length);
    while ((i != full.begin) && (*i != '/') && (*i != '\\'))
    {
        --i;
    }
    return unicode_view_create(full.begin, (size_t)(i - full.begin));
}

unicode_string path_combine(unicode_view const *begin, size_t count)
{
    size_t total_length = 0;
    for (size_t i = 0; i < count; ++i)
    {
        total_length += begin[i].length;
        total_length += 1;
    }
    unicode_string const result = {allocate(total_length), total_length};
    size_t next_write = 0;
    for (size_t i = 0; i < count; ++i)
    {
        unicode_view const piece = begin[i];
        memcpy(result.data + next_write, piece.begin, piece.length);
        next_write += piece.length;
        result.data[next_write] = '/';
        ++next_write;
    }
    if (result.length)
    {
        result.data[result.length - 1] = '\0';
    }
    return result;
}
