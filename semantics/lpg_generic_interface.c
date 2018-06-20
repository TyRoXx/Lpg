#include "lpg_generic_interface.h"
#include "lpg_allocate.h"

void generic_closures_free(generic_closures const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        generic_closure_free(freed.elements[i]);
    }
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

generic_interface generic_interface_create(interface_expression tree, generic_closures closures)
{
    generic_interface const result = {tree, closures};
    return result;
}

void generic_interface_free(generic_interface const freed)
{
    generic_closures_free(freed.closures);
    interface_expression_free(freed.tree);
}
