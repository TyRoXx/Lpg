#include "lpg_generic_interface_instantiation.h"
#include "lpg_allocate.h"

generic_interface_instantiation generic_interface_instantiation_create(generic_interface_id generic, value *arguments,
                                                                       size_t argument_count, interface_id instantiated)
{
    generic_interface_instantiation const result = {generic, arguments, argument_count, instantiated};
    return result;
}

void generic_interface_instantiation_free(generic_interface_instantiation const freed)
{
    deallocate(freed.arguments);
}
