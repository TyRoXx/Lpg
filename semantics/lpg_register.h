#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef uint32_t register_id;

register_id allocate_register(register_id *const used_registers);

typedef struct optional_register_id
{
    bool is_set;
    register_id value;
} optional_register_id;

optional_register_id optional_register_id_create_set(register_id value);
optional_register_id optional_register_id_create_empty(void);
bool optional_register_id_equals(optional_register_id const left, optional_register_id const right);
