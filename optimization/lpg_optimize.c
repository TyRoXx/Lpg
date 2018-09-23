#include "lpg_optimize.h"
#include "lpg_remove_dead_code.h"

void optimize(checked_program *const in_place)
{
    remove_dead_code(in_place);
}
