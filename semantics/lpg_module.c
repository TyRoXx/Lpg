#include "lpg_module.h"

module module_create(unicode_string name, value content, type schema)
{
    module const result = {name, content, schema};
    return result;
}

void module_free(module const freed)
{
    unicode_string_free(&freed.name);
}
