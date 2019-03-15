#include "lpg_ascii.h"

bool is_ascii(char const *const data, size_t const size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if ((unsigned char)data[i] > 127)
        {
            return false;
        }
    }
    return true;
}
