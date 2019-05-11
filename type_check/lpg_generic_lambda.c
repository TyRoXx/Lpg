#include "lpg_generic_lambda.h"
#include "lpg_allocate.h"

generic_lambda generic_lambda_create(lambda tree, generic_closures closures, source_file_owning const *file,
                                     unicode_string current_import_directory)
{
    generic_lambda const result = {tree, closures, file, current_import_directory};
    return result;
}

void generic_lambda_free(generic_lambda const freed)
{
    generic_closures_free(freed.closures);
    lambda_free(&freed.tree);
    unicode_string_free(&freed.current_import_directory);
}
