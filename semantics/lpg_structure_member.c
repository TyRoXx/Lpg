#include "lpg_structure_member.h"

structure_member structure_member_create(type what, unicode_string name,
                                         optional_value compile_time_value)
{
    structure_member result = {what, name, compile_time_value};
    return result;
}

void struct_member_free(structure_member const *value)
{
    unicode_string_free(&value->name);
    type_free(&value->what);
}
