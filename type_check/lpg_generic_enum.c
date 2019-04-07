#include "lpg_generic_enum.h"
#include "lpg_allocate.h"

generic_enum generic_enum_create(enum_expression tree, generic_closures closures,
                                 unicode_string current_import_directory)
{
    generic_enum const result = {tree, closures, current_import_directory};
    return result;
}

void generic_enum_free(generic_enum const freed)
{
    generic_closures_free(freed.closures);
    enum_expression_free(freed.tree);
    unicode_string_free(&freed.current_import_directory);
}
