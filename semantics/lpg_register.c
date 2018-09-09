#include "lpg_register.h"

register_id allocate_register(register_id *const used_registers)
{
    register_id const id = *used_registers;
    ++(*used_registers);
    return id;
}

optional_register_id optional_register_id_create_set(register_id value)
{
    optional_register_id const result = {true, value};
    return result;
}

optional_register_id optional_register_id_create_empty(void)
{
    optional_register_id const result = {false, ~(register_id)0};
    return result;
}

bool optional_register_id_equals(optional_register_id const left, optional_register_id const right)
{
    if (left.is_set)
    {
        if (right.is_set)
        {
            return (left.value == right.value);
        }
        return false;
    }
    else
    {
        if (right.is_set)
        {
            return false;
        }
        return true;
    }
}
