#include "lpg_generic_impl_regular_interface.h"

generic_impl_regular_interface generic_impl_regular_interface_create(interface_id interface_, impl_expression tree,
                                                                     generic_closures closures,
                                                                     generic_instantiation_expression self)
{
    generic_impl_regular_interface const result = {interface_, tree, closures, self};
    return result;
}

void generic_impl_regular_interface_free(generic_impl_regular_interface const freed)
{
    impl_expression_free(freed.tree);
    generic_closures_free(freed.closures);
    generic_instantiation_expression_free(freed.self);
}
