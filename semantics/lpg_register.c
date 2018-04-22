#include "lpg_register.h"

register_id allocate_register(register_id *const used_registers)
{
    register_id const id = *used_registers;
    ++(*used_registers);
    return id;
}
