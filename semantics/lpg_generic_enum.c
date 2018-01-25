#include "lpg_generic_enum.h"
#include "lpg_allocate.h"

generic_enum_closure generic_enum_closure_create(unicode_string name, type what, value content)
{
    generic_enum_closure const result = {name, what, content};
    return result;
}

void generic_enum_closure_free(generic_enum_closure const freed)
{
    unicode_string_free(&freed.name);
}

generic_enum generic_enum_create(enum_expression tree, generic_enum_closure *closures, size_t closure_count)
{
    generic_enum const result = {tree, closures, closure_count};
    return result;
}

void generic_enum_free(generic_enum const freed)
{
    for (size_t i = 0; i < freed.closure_count; ++i)
    {
        generic_enum_closure_free(freed.closures[i]);
    }
    if (freed.closures)
    {
        deallocate(freed.closures);
    }
}
