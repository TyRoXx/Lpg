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
    for (size_t i = 0; i < program->interface_count; ++i)
    {
        interface_free(program->interfaces[i]);
    }
    if (program->interfaces)
    {
        deallocate(program->interfaces);
    }
    garbage_collector_free(program->memory);
}
