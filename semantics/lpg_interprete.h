#pragma once
#include "lpg_value.h"
#include "lpg_checked_program.h"

value call_function(value const callee, value const *const inferred,
                    value *const arguments, value const *globals);

void interprete(checked_program const program, value const *globals);
