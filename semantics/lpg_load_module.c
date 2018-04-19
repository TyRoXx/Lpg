#include "lpg_load_module.h"

module_loader module_loader_create(void)
{
    module_loader result = {NULL};
    return result;
}

load_module_result load_module(module_loader *loader, unicode_view name)
{
    (void)loader;
    if (unicode_view_equals_c_str(name, "std"))
    {
        load_module_result const success = {optional_value_create(value_from_unit()), type_from_unit()};
        return success;
    }
    load_module_result const failure = {optional_value_empty, type_from_unit()};
    return failure;
}
