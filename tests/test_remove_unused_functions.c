#include "test_remove_unused_functions.h"
#include "test.h"
#include "lpg_checked_program.h"
#include "lpg_standard_library.h"
#include "lpg_remove_unused_functions.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"

void test_remove_unused_functions(void)
{
    standard_library_description const std_library = describe_standard_library();
    function_id const original_function_count = 3;
    checked_program original = {
        NULL, 0, {NULL}, allocate_array(original_function_count, sizeof(*original.functions)), original_function_count};

    for (function_id i = 0; i < original.function_count; ++i)
    {
        original.functions[i].signature = allocate(sizeof(*original.functions[i].signature));
        *original.functions[i].signature =
            function_pointer_create(type_from_unit(), tuple_type_create(NULL, 0), tuple_type_create(NULL, 0));
        original.functions[i].body = instruction_sequence_create(NULL, 0);
        original.functions[i].number_of_registers = 0;
        original.functions[i].return_value = 0;
    }

    {
        size_t const body_size = 1;
        instruction *const body = allocate_array(body_size, sizeof(*body));
        body[0] = instruction_create_literal(
            literal_instruction_create(0, value_from_function_pointer(function_pointer_value_from_internal(2, NULL, 0)),
                                       type_from_function_pointer(original.functions[2].signature)));
        original.functions[0].body = instruction_sequence_create(body, body_size);
    }

    REQUIRE(original.function_count == original_function_count);
    checked_program optimized = remove_unused_functions(original);
    REQUIRE(optimized.function_count == 2);
    {
        instruction_sequence expected_main;
        {
            size_t const body_size = 1;
            instruction *const body = allocate_array(body_size, sizeof(*body));
            body[0] = instruction_create_literal(literal_instruction_create(
                0, value_from_function_pointer(function_pointer_value_from_internal(1, NULL, 0)),
                type_from_function_pointer(optimized.functions[1].signature)));
            expected_main = instruction_sequence_create(body, body_size);
        }
        REQUIRE(instruction_sequence_equals(&expected_main, &optimized.functions[0].body));
        instruction_sequence_free(&expected_main);
    }
    REQUIRE(instruction_sequence_equals(&original.functions[2].body, &optimized.functions[1].body));
    checked_program_free(&optimized);
    checked_program_free(&original);
    standard_library_description_free(&std_library);
}
