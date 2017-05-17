#include "lpg_checked_function.h"

void checked_function_free(checked_function const *function)
{
    instruction_sequence_free(&function->body);
}
