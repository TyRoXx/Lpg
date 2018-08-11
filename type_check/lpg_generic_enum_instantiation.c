#include "lpg_generic_enum_instantiation.h"
#include "lpg_allocate.h"

generic_enum_instantiation generic_enum_instantiation_create(generic_enum_id generic, value *arguments,
                                                             size_t argument_count, enum_id instantiated)
{
    generic_enum_instantiation const result = {generic, arguments, argument_count, instantiated};
    return result;
}

void generic_enum_instantiation_free(generic_enum_instantiation const freed)
{
    deallocate(freed.arguments);
}
