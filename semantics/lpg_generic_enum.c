#include "lpg_generic_enum.h"

generic_enum generic_enum_create(enum_expression tree)
{
    generic_enum const result = {tree};
    return result;
}
