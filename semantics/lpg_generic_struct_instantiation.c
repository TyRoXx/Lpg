#include "lpg_generic_struct_instantiation.h"
#include "lpg_allocate.h"

generic_struct_instantiation generic_struct_instantiation_create(generic_struct_id generic, value *arguments,
                                                                 size_t argument_count, struct_id instantiated)
{
    generic_struct_instantiation const result = {generic, arguments, argument_count, instantiated};
    return result;
}

void generic_struct_instantiation_free(generic_struct_instantiation const freed)
{
    deallocate(freed.arguments);
}
