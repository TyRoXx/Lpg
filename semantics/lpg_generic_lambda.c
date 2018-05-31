#include "lpg_generic_lambda.h"
#include "lpg_allocate.h"

generic_lambda generic_lambda_create(lambda tree, generic_enum_closures closures)
{
    generic_lambda const result = {tree, closures};
    return result;
}

void generic_lambda_free(generic_lambda const freed)
{
    generic_enum_closures_free(freed.closures);
    lambda_free(&freed.tree);
}
