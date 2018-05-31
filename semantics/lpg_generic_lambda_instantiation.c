#include "lpg_generic_lambda_instantiation.h"
#include "lpg_allocate.h"

generic_lambda_instantiation generic_lambda_instantiation_create(generic_lambda_id generic, value *arguments,
                                                                 size_t argument_count, function_id instantiated)
{
    generic_lambda_instantiation const result = {generic, arguments, argument_count, instantiated};
    return result;
}

void generic_lambda_instantiation_free(generic_lambda_instantiation const freed)
{
    deallocate(freed.arguments);
}
