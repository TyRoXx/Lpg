#pragma once
#include <stdint.h>

typedef uint32_t register_id;

register_id allocate_register(register_id *const used_registers);
