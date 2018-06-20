#include "lpg_optimize.h"
#include "lpg_remove_dead_code.h"
#include "lpg_remove_unused_functions.h"

void optimize(checked_program *const in_place)
{
    remove_dead_code(in_place);
    checked_program const optimized = remove_unused_functions(*in_place);
    checked_program_free(in_place);
    *in_place = optimized;
}
