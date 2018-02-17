#pragma once
#include "lpg_value.h"

void print_value(value const printed, size_t const indentation);
void print_instruction(instruction const printed, size_t const indentation);
void print_instruction_sequence(instruction_sequence const sequence, size_t const indentation);
