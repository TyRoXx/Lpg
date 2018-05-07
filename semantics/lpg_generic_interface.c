#include "lpg_generic_interface.h"
#include "lpg_allocate.h"

void generic_enum_closures_free(generic_enum_closures const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        generic_enum_closure_free(freed.elements[i]);
    }
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

generic_interface generic_interface_create(interface_expression tree, generic_enum_closures closures)
{
    generic_interface const result = {tree, closures};
    return result;
}

void generic_interface_free(generic_interface const freed)
{
    generic_enum_closures_free(freed.closures);
}
