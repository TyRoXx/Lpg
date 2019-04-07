#include "lpg_generic_impl.h"
#include "lpg_allocate.h"

generic_closure generic_closure_create(unicode_string name, type what, value content)
{
    generic_closure const result = {name, what, content};
    return result;
}

void generic_closure_free(generic_closure const freed)
{
    unicode_string_free(&freed.name);
}

generic_closures generic_closures_create(generic_closure *elements, size_t count)
{
    generic_closures const result = {elements, count};
    return result;
}

void generic_closures_free(generic_closures const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        generic_closure_free(freed.elements[i]);
    }
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

generic_impl_self generic_impl_self_create_regular(type regular)
{
    generic_impl_self const result = {true, regular, generic_instantiation_expression_create(NULL, NULL, 0)};
    return result;
}

generic_impl_self generic_impl_self_create_generic(generic_instantiation_expression generic)
{
    generic_impl_self const result = {false, type_from_unit(), generic};
    return result;
}

void generic_impl_self_free(generic_impl_self const freed)
{
    if (freed.is_regular)
    {
        return;
    }
    generic_instantiation_expression_free(freed.generic);
}

generic_impl generic_impl_create(impl_expression tree, generic_closures closures, generic_impl_self self,
                                 source_file_owning source, unicode_string current_import_directory)
{
    generic_impl const result = {tree, closures, self, source, current_import_directory};
    return result;
}

void generic_impl_free(generic_impl const freed)
{
    generic_closures_free(freed.closures);
    impl_expression_free(freed.tree);
    generic_impl_self_free(freed.self);
    source_file_owning_free(freed.source);
    unicode_string_free(&freed.current_import_directory);
}
