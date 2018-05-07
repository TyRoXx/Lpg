#include "lpg_program_check.h"
#include "lpg_allocate.h"

void program_check_free(program_check const freed)
{
    for (size_t i = 0; i < freed.generic_enum_count; ++i)
    {
        generic_enum_free(freed.generic_enums[i]);
    }
    if (freed.generic_enums)
    {
        deallocate(freed.generic_enums);
    }
    for (size_t i = 0; i < freed.generic_interface_count; ++i)
    {
        generic_interface_free(freed.generic_interfaces[i]);
    }
    if (freed.generic_interfaces)
    {
        deallocate(freed.generic_interfaces);
    }
    for (size_t i = 0; i < freed.enum_instantiation_count; ++i)
    {
        generic_enum_instantiation_free(freed.enum_instantiations[i]);
    }
    if (freed.enum_instantiations)
    {
        deallocate(freed.enum_instantiations);
    }
    for (size_t i = 0; i < freed.module_count; ++i)
    {
        module_free(freed.modules[i]);
    }
    if (freed.modules)
    {
        deallocate(freed.modules);
    }
}

void begin_load_module(program_check *to, unicode_string name)
{
    to->modules = reallocate_array(to->modules, to->module_count + 1, sizeof(*to->modules));
    to->modules[to->module_count] = module_create(name, optional_value_empty, optional_type_create_empty());
    ++(to->module_count);
}

static module *find_module(program_check *check, unicode_view const name)
{
    for (size_t i = 0; i < check->module_count; ++i)
    {
        if (unicode_view_equals(name, unicode_view_from_string(check->modules[i].name)))
        {
            return &check->modules[i];
        }
    }
    return NULL;
}

void fail_load_module(program_check *to, unicode_view const name)
{
    module *const found = find_module(to, name);
    ASSUME(found);
    module_free(*found);
    *found = to->modules[to->module_count - 1];
    to->module_count -= 1;
}

void succeed_load_module(program_check *to, unicode_view const name, value const content, type const schema)
{
    module *const found = find_module(to, name);
    ASSUME(found);
    found->content = optional_value_create(content);
    found->schema = optional_type_create_set(schema);
}
