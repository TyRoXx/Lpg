#include "lpg_generic_enum.h"
#include "lpg_allocate.h"

generic_enum generic_enum_create(enum_expression tree, generic_closures closures)
{
    generic_enum const result = {tree, closures};
    return result;
}

void generic_enum_free(generic_enum const freed)
{
    generic_closures_free(freed.closures);
    enum_expression_free(freed.tree);
}
