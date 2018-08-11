#include "lpg_generic_interface.h"
#include "lpg_allocate.h"

generic_interface generic_interface_create(interface_expression tree, generic_closures closures)
{
    generic_interface const result = {tree, closures, NULL, 0};
    return result;
}

void generic_interface_free(generic_interface const freed)
{
    generic_closures_free(freed.closures);
    interface_expression_free(freed.tree);
    for (size_t i = 0; i < freed.generic_impl_count; ++i)
    {
        generic_impl_free(freed.generic_impls[i]);
    }
    if (freed.generic_impls)
    {
        deallocate(freed.generic_impls);
    }
}
