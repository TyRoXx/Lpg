#include "lpg_generic_impl_regular_interface.h"
#include "lpg_allocate.h"

generic_impl_regular_interface generic_impl_regular_interface_create(interface_id interface_, impl_expression tree,
                                                                     generic_closures closures,
                                                                     generic_instantiation_expression self,
                                                                     source_file source)
{
    generic_impl_regular_interface const result = {interface_, tree, closures, self, NULL, 0, source};
    return result;
}

void generic_impl_regular_interface_free(generic_impl_regular_interface const freed)
{
    impl_expression_free(freed.tree);
    generic_closures_free(freed.closures);
    generic_instantiation_expression_free(freed.self);
    if (freed.instantiations)
    {
        deallocate(freed.instantiations);
    }
}
