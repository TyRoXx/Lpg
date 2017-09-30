#include "lpg_arithmetic.h"

optional_size make_optional_size(size_t const value)
{
    optional_size const result = {optional_set, value};
    return result;
}

optional_size size_multiply(size_t const first, size_t const second)
{
    size_t const product = first * second;
    if ((first != 0) && (product / first) != second)
    {
        return optional_size_empty;
    }
    return make_optional_size(product);
}
