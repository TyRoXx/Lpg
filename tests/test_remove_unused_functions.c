#include "test_remove_unused_functions.h"
#include "test.h"
#include "lpg_checked_program.h"
#include "lpg_standard_library.h"
#include "lpg_remove_unused_functions.h"
#include "lpg_allocate.h"

void test_remove_unused_functions(void)
{
    standard_library_description const std_library =
        describe_standard_library();
    checked_program original = {
        {NULL}, allocate_array(2, sizeof(*original.functions)), 2};

    for (function_id i = 0; i < original.function_count; ++i)
    {
        original.functions[i].signature =
            allocate(sizeof(*original.functions[i].signature));
        *original.functions[i].signature =
            function_pointer_create(type_from_unit(), NULL, 0);
        original.functions[i].body = instruction_sequence_create(NULL, 0);
        original.functions[i].number_of_registers = 0;
        original.functions[i].return_value = 0;
    }

    REQUIRE(original.function_count == 2);
    checked_program optimized = remove_unused_functions(original);
    REQUIRE(optimized.function_count == 1);
    checked_program_free(&optimized);
    checked_program_free(&original);
    standard_library_description_free(&std_library);
}
