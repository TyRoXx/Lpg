#include "lpg_checked_program.h"
#include "lpg_for.h"
#include "lpg_allocate.h"

void checked_program_free(checked_program const *program)
{
    LPG_FOR(size_t, i, program->function_count)
    {
        checked_function_free(program->functions + i);
    }
    if (program->functions)
    {
        deallocate(program->functions);
    }
    garbage_collector_free(program->memory);
}
