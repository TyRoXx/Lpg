#include "lpg_identifier.h"

bool is_identifier_begin(char const c)
{
    if ((c >= 'a') && (c <= 'z'))
    {
        return 1;
    }
    if ((c >= 'A') && (c <= 'Z'))
    {
        return 1;
    }
    return (c == '_');
}

bool is_identifier_middle(char const c)
{
    if (is_identifier_begin(c))
    {
        return 1;
    }
    if ((c >= '0') && (c <= '9'))
    {
        return 1;
    }
    return (c == '-');
}

bool is_identifier(char const *data, size_t size)
{
    if (size < 1)
    {
        return 0;
    }
    if (!is_identifier_begin(data[0]))
    {
        return 0;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (!is_identifier_middle(data[i]))
        {
            return 0;
        }
    }
    return 1;
}
