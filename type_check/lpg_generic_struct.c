#include "lpg_generic_struct.h"
#include "lpg_allocate.h"

generic_struct generic_struct_create(struct_expression tree, generic_closures closures,
                                     unicode_string current_import_directory)
{
    generic_struct const result = {tree, closures, current_import_directory};
    return result;
}

void generic_struct_free(generic_struct const freed)
{
    generic_closures_free(freed.closures);
    struct_expression_free(&freed.tree);
    unicode_string_free(&freed.current_import_directory);
}
