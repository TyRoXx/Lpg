#include "lpg_local_variable.h"
#include "lpg_allocate.h"
#include "lpg_for.h"

local_variable local_variable_create(unicode_string name, type const type_,
                                     optional_value compile_time_value,
                                     register_id where)
{
    local_variable const result = {name, type_, compile_time_value, where};
    return result;
}

void local_variable_free(local_variable const *const value)
{
    unicode_string_free(&value->name);
}

void add_local_variable(local_variable_container *to, local_variable variable)
{
    to->elements =
        reallocate_array(to->elements, to->count + 1, sizeof(*to->elements));
    to->elements[to->count] = variable;
    ++(to->count);
}

bool local_variable_name_exists(local_variable_container const variables,
                                unicode_view const name)
{
    LPG_FOR(size_t, i, variables.count)
    {
        if (unicode_view_equals(
                unicode_view_from_string(variables.elements[i].name), name))
        {
            return true;
        }
    }
    return false;
}
